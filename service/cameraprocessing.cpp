
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QImage>
#include <QThread>
#include <QCoreApplication>
#include <QRegularExpression>

#include "cameraprocessing.h"

QMap<QString, QString> vendorMap;
QMap<QPair<QString, QString>, QString> deviceMap;

void initializeWMF() {
    HRESULT status = MFStartup(MF_VERSION);
    if (FAILED(status)) {
        qCritical() << "Failed to initialize WMF";
        return;
    }
}

void deinitializeWMF() {
    HRESULT status = MFShutdown();
    if (FAILED(status)) {
        qCritical() << "Failed to deinitialize WMF";
        return;
    }
}

IMFAttributes* createCaptureAttributes(const GUID& sourceType) {
    IMFAttributes* attributes = nullptr;
    HRESULT status = MFCreateAttributes(&attributes, 1);
    if (FAILED(status)) {
        qCritical() << "Failed to create WMF attributes.";
        return nullptr;
    }
    status = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, sourceType);
    if (FAILED(status)) {
        qCritical() << "Failed to set attribute for capture devices.";
        attributes->Release();
        return nullptr;
    }
    return attributes;
}

void deleteAttributes(IMFAttributes* attributes) {
    if (attributes != nullptr) {
        attributes->Release();
        attributes = nullptr;
    }
}

IMFActivate** enumerateCaptureDevices(IMFAttributes* attributes, UINT32& deviceCount) {
    IMFActivate** devices = nullptr;
    deviceCount = 0;
    if (!attributes) {
        qCritical() << "Attributes pointer is null.";
        return nullptr;
    }
    HRESULT status = MFEnumDeviceSources(attributes, &devices, &deviceCount);
    if (FAILED(status)) {
        qCritical() << "Failed to enumerate capture devices.";
        return nullptr;
    }
    if (deviceCount == 0) {
        qWarning() << "No capture devices found.";
        return nullptr;
    }
    return devices;
}

void deleteDeviceList(IMFActivate** devices, UINT32 deviceCount) {
    if (devices == nullptr) {
        return;
    }
    for (UINT32 i = 0; i < deviceCount; ++i) {
        if (devices[i]) {
            devices[i]->Release();
        }
    }
    CoTaskMemFree(devices);
    devices = nullptr;
}

QString getDeviceName(IMFActivate* device) {
    WCHAR* name = nullptr;
    UINT32 nameLength = 0;
    HRESULT status = device->GetAllocatedString(
        MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &nameLength);
    if (FAILED(status)) {
        qWarning() << "Failed to retrieve device name.";
        return QString();
    }
    QString result = QString::fromWCharArray(name);
    CoTaskMemFree(name);
    return result;
}

QString getCodecName(const GUID& guid) {
    if (guid == MFVideoFormat_NV12) return "NV12";
    if (guid == MFVideoFormat_MJPG) return "MJPG";
    if (guid == MFVideoFormat_YUY2) return "YUY2";
    if (guid == MFVideoFormat_RGB24) return "RGB24";
    if (guid == MFVideoFormat_I420) return "I420";
    if (guid == MFAudioFormat_PCM) return "PCM";
    if (guid == MFAudioFormat_Float) return "IEEE Float";
    WCHAR* guidName = nullptr;
    StringFromCLSID(guid, &guidName);
    QString codec = QString::fromWCharArray(guidName);
    CoTaskMemFree(guidName);
    return codec;
}

QString getDeviceSymbolicLink(IMFActivate* device) {
    WCHAR* symbolicLink = nullptr;
    UINT32 linkLength = 0;
    HRESULT status = S_OK;
    GUID guidSymbolicLink = GUID_NULL;
    UINT32 attrCount = 0;
    status = device->GetCount(&attrCount);
    if (FAILED(status)) {
        qWarning() << "Failed to get attribute count.";
        return QString();
    }
    for (UINT32 i = 0; i < attrCount; ++i) {
        GUID guidKey;
        PROPVARIANT var;
        PropVariantInit(&var);
        status = device->GetItemByIndex(i, &guidKey, &var);
        if (SUCCEEDED(status)) {
            if (guidKey == MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK ||
                guidKey == MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_SYMBOLIC_LINK) {
                guidSymbolicLink = guidKey;
                PropVariantClear(&var);
                break;
            }
            PropVariantClear(&var);
        }
    }
    if (guidSymbolicLink == GUID_NULL) {
        qWarning() << "Symbolic link attribute not found.";
        return QString();
    }
    status = device->GetAllocatedString(
        guidSymbolicLink, &symbolicLink, &linkLength);
    if (FAILED(status)) {
        qWarning() << "Failed to retrieve device symbolic link.";
        return QString();
    }
    QString result = QString::fromWCharArray(symbolicLink);
    CoTaskMemFree(symbolicLink);
    return result;
}

QString getDeviceID(IMFActivate* device) {
    QString deviceString = getDeviceSymbolicLink(device);
    static const QRegularExpression regex(R"(pid_([0-9A-Fa-f]{4}))");
    QRegularExpressionMatch match = regex.match(deviceString);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    qWarning() << "Device ID not found in device string:" << deviceString;
    return QString();
}

QString getVendorID(IMFActivate* device) {
    QString deviceString = getDeviceSymbolicLink(device);
    static const QRegularExpression regex(R"(vid_([0-9A-Fa-f]{4}))");
    QRegularExpressionMatch match = regex.match(deviceString);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    qWarning() << "Vendor ID not found in device string:" << deviceString;
    return QString();
}

QList<QString> getAvailableVideoCodecs(IMFActivate* videoDevice) {
    QList<QString> codecsList;
    if (!videoDevice) {
        qWarning() << "Video device is null.";
        return codecsList;
    }
    IMFMediaSource* videoSource = createMediaSource(videoDevice);
    if (!videoSource) {
        qWarning() << "Failed to create media source from video device.";
        return codecsList;
    }
    IMFSourceReader* sourceReader = nullptr;
    HRESULT status = MFCreateSourceReaderFromMediaSource(videoSource, nullptr, &sourceReader);
    if (FAILED(status)) {
        qWarning() << "Failed to create source reader for video device.";
        videoSource->Release();
        return codecsList;
    }
    DWORD mediaTypeIndex = 0;
    IMFMediaType* mediaType = nullptr;
    while (SUCCEEDED(sourceReader->GetNativeMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, mediaTypeIndex, &mediaType))) {
        GUID subtype = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
        UINT32 width = 0, height = 0, fpsNumerator = 0, fpsDenominator = 0;
        mediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
        MFGetAttributeSize(mediaType, MF_MT_FRAME_SIZE, &width, &height);
        MFGetAttributeRatio(mediaType, MF_MT_FRAME_RATE, &fpsNumerator, &fpsDenominator);
        QString codecName = getCodecName(subtype);
        QString codecInfo = QString("Codec: %1, Resolution: %2x%3, FPS: %4")
                                .arg(codecName)
                                .arg(width)
                                .arg(height)
                                .arg(fpsDenominator != 0 ? (double)fpsNumerator / fpsDenominator : 0);

        codecsList.append(codecInfo);
        mediaType->Release();
        mediaTypeIndex++;
    }
    sourceReader->Release();
    videoSource->Release();
    return codecsList;
}

QList<QString> getAvailableAudioCodecs(IMFActivate* audioDevice) {
    QList<QString> codecsList;
    if (!audioDevice) {
        qWarning() << "Audio device is null.";
        return codecsList;
    }
    IMFMediaSource* audioSource = createMediaSource(audioDevice);
    if (!audioSource) {
        qWarning() << "Failed to create media source from audio device.";
        return codecsList;
    }
    IMFSourceReader* sourceReader = nullptr;
    HRESULT status = MFCreateSourceReaderFromMediaSource(audioSource, nullptr, &sourceReader);
    if (FAILED(status)) {
        qWarning() << "Failed to create source reader for audio device.";
        audioSource->Release();
        return codecsList;
    }
    DWORD mediaTypeIndex = 0;
    IMFMediaType* mediaType = nullptr;
    while (SUCCEEDED(sourceReader->GetNativeMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, mediaTypeIndex, &mediaType))) {
        GUID subtype = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
        UINT32 sampleRate = 0;
        UINT32 channels = 0;
        UINT32 bitsPerSample = 0;
        mediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
        mediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
        mediaType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
        mediaType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);
        QString codecName = getCodecName(subtype);
        QString codecInfo = QString("Codec: %1, Sample Rate: %2, Channels: %3, Bits Per Sample: %4")
                                .arg(codecName)
                                .arg(sampleRate)
                                .arg(channels)
                                .arg(bitsPerSample);

        codecsList.append(codecInfo);
        mediaType->Release();
        mediaTypeIndex++;
    }
    sourceReader->Release();
    audioSource->Release();
    return codecsList;
}

bool areDevicesLinked(IMFActivate* videoDevice, IMFActivate* audioDevice) {
    WCHAR* videoSymbolicLink = nullptr;
    WCHAR* audioSymbolicLink = nullptr;
    HRESULT videoStatus = videoDevice->GetAllocatedString(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
        &videoSymbolicLink, nullptr);
    HRESULT audioStatus = audioDevice->GetAllocatedString(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_SYMBOLIC_LINK,
        &audioSymbolicLink, nullptr);
    if (FAILED(videoStatus) || FAILED(audioStatus)) {
        CoTaskMemFree(videoSymbolicLink);
        CoTaskMemFree(audioSymbolicLink);
        return false;
    }
    QString videoLink = QString::fromWCharArray(videoSymbolicLink);
    QString audioLink = QString::fromWCharArray(audioSymbolicLink);
    CoTaskMemFree(videoSymbolicLink);
    CoTaskMemFree(audioSymbolicLink);
    return videoLink.contains(audioLink, Qt::CaseInsensitive);
}

IMFMediaSource* createMediaSource(IMFActivate* device) {
    if (device == nullptr) {
        qCritical() << "Device is null.";
        return nullptr;
    }
    IMFMediaSource* mediaSource = nullptr;
    HRESULT status = device->ActivateObject(
        __uuidof(IMFMediaSource), (void**)&mediaSource);
    if (FAILED(status)) {
        qCritical() << "Failed to activate media source";
        return nullptr;
    }
    return mediaSource;
}

void deleteMediaSource(IMFMediaSource* mediaSource) {
    if (mediaSource != nullptr) {
        mediaSource->Release();
        mediaSource = nullptr;
    }
}

IMFSourceReader* createSourceReader(IMFMediaSource* mediaSource) {
    if (!mediaSource) {
        qCritical() << "Media source is null.";
        return nullptr;
    }
    IMFSourceReader* sourceReader = nullptr;
    HRESULT status = MFCreateSourceReaderFromMediaSource(
        mediaSource, nullptr, &sourceReader);
    if (FAILED(status)) {
        qCritical() << "Failed to create source reader";
        return nullptr;
    }
    return sourceReader;
}

void deleteSourceReader(IMFSourceReader* sourceReader) {
    if (sourceReader) {
        sourceReader->Release();
        sourceReader = nullptr;
    }
}

IMFSample* readSampleFromSourceReader(IMFSourceReader* sourceReader,
                                      DWORD streamType, int maxAttempts) {
    if (!sourceReader) {
        qCritical() << "Source reader is null.";
        return nullptr;
    }
    IMFSample* sample = nullptr;
    DWORD streamIndex = 0;
    DWORD flags = 0;
    LONGLONG timestamp = 0;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        HRESULT status = sourceReader->ReadSample(
            streamType, 0, &streamIndex, &flags, &timestamp, &sample);
        if (SUCCEEDED(status)) {
            if (flags & MF_SOURCE_READERF_STREAMTICK) {
                qWarning() << "Stream tick received. Attempt:" << attempt;
                QThread::msleep(100);
                continue;
            }
            if (sample) {
                qDebug() << "Sample successfully read. Timestamp:" << timestamp;
                return sample;
            }
        } else {
            qCritical() << "Failed to read sample. HRESULT:"
                        << QString("0x%1").arg(status, 0, 16);
            QThread::msleep(100);
        }
    }
    qCritical() << "No sample available after" << maxAttempts << "attempts.";
    return nullptr;
}

bool configureSourceReaderVideoFormat(IMFSourceReader* sourceReader,
                                      UINT32 width, UINT32 height, UINT32 fps) {
    if (!sourceReader) {
        qCritical() << "Source reader is null.";
        return false;
    }
    IMFMediaType* mediaType = nullptr;
    HRESULT status = MFCreateMediaType(&mediaType);
    if (FAILED(status)) {
        qCritical() << "Failed to create media type.";
        return false;
    }
    status = mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    status |= mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_MJPG);
    status |= MFSetAttributeSize(mediaType, MF_MT_FRAME_SIZE, width, height);
    status |= MFSetAttributeRatio(mediaType, MF_MT_FRAME_RATE, fps, 1);
    if (FAILED(status)) {
        qCritical() << "Failed to configure media type.";
        mediaType->Release();
        return false;
    }
    status = sourceReader->SetCurrentMediaType(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, mediaType);
    mediaType->Release();
    if (FAILED(status)) {
        qCritical() << "Failed to set current media type for source reader.";
        return false;
    }
    return true;
}

bool configureSourceReaderAudioFormat(IMFSourceReader* sourceReader,
                                      UINT32 sampleRate, UINT32 channels,
                                      UINT32 bitsPerSample) {
    if (!sourceReader) {
        qCritical() << "Source reader is null.";
        return false;
    }
    IMFMediaType* mediaType = nullptr;
    HRESULT status = MFCreateMediaType(&mediaType);
    if (FAILED(status)) {
        qCritical() << "Failed to create media type.";
        return false;
    }
    status = mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    status |= mediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    status |= mediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
    status |= mediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels);
    status |= mediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitsPerSample);
    if (FAILED(status)) {
        qCritical() << "Failed to configure audio media type.";
        mediaType->Release();
        return false;
    }
    status = sourceReader->SetCurrentMediaType(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, mediaType);
    mediaType->Release();
    if (FAILED(status)) {
        qCritical() << "Failed to set current media type for audio source reader.";
        return false;
    }
    return true;
}

IMFSinkWriter* createSinkWriter(const QString& outputPath) {
    IMFSinkWriter* sinkWriter = nullptr;
    IMFAttributes* attributes = nullptr;
    HRESULT status = MFCreateAttributes(&attributes, 1);
    if (FAILED(status)) {
        qCritical() << "Failed to create attributes for Sink Writer.";
        return nullptr;
    }
    status = attributes->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_AVI);
    if (FAILED(status)) {
        qCritical() << "Failed to set container type to AVI.";
        attributes->Release();
        return nullptr;
    }
    status = MFCreateSinkWriterFromURL((LPCWSTR)outputPath.utf16(), nullptr, attributes, &sinkWriter);
    attributes->Release();
    if (FAILED(status)) {
        qCritical() << "Failed to create Sink Writer. HRESULT:"
                    << QString("0x%1").arg(status, 0, 16);
        return nullptr;
    }
    return sinkWriter;
}

bool configureOutputFormat(IMFSinkWriter* sinkWriter, DWORD& streamIndex,
                           UINT32 width, UINT32 height, UINT32 fps) {
    if (!sinkWriter) {
        qCritical() << "Sink Writer is null.";
        return false;
    }
    IMFMediaType* outputMediaType = nullptr;
    HRESULT status = MFCreateMediaType(&outputMediaType);
    if (FAILED(status)) {
        qCritical() << "Failed to create output media type.";
        return false;
    }
    status = outputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    status |= outputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_MJPG);
    status |= MFSetAttributeSize(outputMediaType, MF_MT_FRAME_SIZE, width, height);
    status |= MFSetAttributeRatio(outputMediaType, MF_MT_FRAME_RATE, fps, 1);
    status |= MFSetAttributeRatio(outputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (FAILED(status)) {
        qCritical() << "Failed to configure output media type.";
        outputMediaType->Release();
        return false;
    }
    status = sinkWriter->AddStream(outputMediaType, &streamIndex);
    outputMediaType->Release();
    if (FAILED(status)) {
        qCritical() << "Failed to add output stream.";
        return false;
    }
    return true;
}

bool configureInputFormat(IMFSinkWriter* sinkWriter, DWORD streamIndex,
                          UINT32 width, UINT32 height, UINT32 fps) {
    if (!sinkWriter) {
        qCritical() << "Sink Writer is null.";
        return false;
    }
    IMFMediaType* inputMediaType = nullptr;
    HRESULT status = MFCreateMediaType(&inputMediaType);
    if (FAILED(status)) {
        qCritical() << "Failed to create input media type.";
        return false;
    }
    status = inputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    status |= inputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_MJPG);
    status |= MFSetAttributeSize(inputMediaType, MF_MT_FRAME_SIZE, width, height);
    status |= MFSetAttributeRatio(inputMediaType, MF_MT_FRAME_RATE, fps, 1);
    status |= MFSetAttributeRatio(inputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (FAILED(status)) {
        qCritical() << "Failed to configure input media type. HRESULT:"
                    << QString("0x%1").arg(status, 0, 16);
        inputMediaType->Release();
        return false;
    }
    status = sinkWriter->SetInputMediaType(streamIndex, inputMediaType, nullptr);
    inputMediaType->Release();
    if (FAILED(status)) {
        _com_error err(status);
        qCritical() << "Failed to set input media type. HRESULT:"
                    << QString("0x%1").arg(status, 0, 16)
                    << "Error:" << err.ErrorMessage();
        return false;
    }
    return true;
}

bool configureAudioOutputFormat(IMFSinkWriter* sinkWriter, DWORD& audioStreamIndex,
                                UINT32 sampleRate, UINT32 channels) {
    if (!sinkWriter) {
        qCritical() << "Sink Writer is null.";
        return false;
    }
    IMFMediaType* outputAudioType = nullptr;
    HRESULT status = MFCreateMediaType(&outputAudioType);
    if (FAILED(status)) {
        qCritical() << "Failed to create output audio media type.";
        return false;
    }
    status = outputAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    status |= outputAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    status |= outputAudioType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
    status |= outputAudioType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels);
    status |= outputAudioType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    if (FAILED(status)) {
        qCritical() << "Failed to configure output audio format.";
        outputAudioType->Release();
        return false;
    }
    status = sinkWriter->AddStream(outputAudioType, &audioStreamIndex);
    outputAudioType->Release();
    if (FAILED(status)) {
        qCritical() << "Failed to add audio stream to Sink Writer.";
        return false;
    }
    return true;
}

bool configureAudioInputFormat(IMFSinkWriter* sinkWriter, DWORD audioStreamIndex,
                               UINT32 sampleRate, UINT32 channels,
                               UINT32 bitsPerSample) {
    if (!sinkWriter) {
        qCritical() << "Sink Writer is null.";
        return false;
    }
    IMFMediaType* inputAudioType = nullptr;
    HRESULT status = MFCreateMediaType(&inputAudioType);
    if (FAILED(status)) {
        qCritical() << "Failed to create input audio media type.";
        return false;
    }
    status |= inputAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    status |= inputAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    status |= inputAudioType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
    status |= inputAudioType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels);
    status |= inputAudioType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitsPerSample);
    status |= inputAudioType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT,
                                        channels * bitsPerSample / 8);
    status |= inputAudioType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,
                                        sampleRate * channels * bitsPerSample / 8);
    if (FAILED(status)) {
        qCritical() << "Failed to configure input audio format.";
        inputAudioType->Release();
        return false;
    }
    status = sinkWriter->SetInputMediaType(audioStreamIndex,
                                           inputAudioType, nullptr);
    inputAudioType->Release();
    if (FAILED(status)) {
        qCritical() << "Failed to set input audio media type.";
        return false;
    }

    return true;
}
bool beginSinkWriter(IMFSinkWriter* sinkWriter) {
    if (!sinkWriter) {
        qCritical() << "Sink Writer is null.";
        return false;
    }
    HRESULT status = sinkWriter->BeginWriting();
    if (FAILED(status)) {
        qCritical() << "Failed to begin writing with Sink Writer.";
        return false;
    }
    return true;
}
bool writeSample(IMFSinkWriter* sinkWriter, DWORD streamIndex,
                 IMFSample* sample) {
    if (!sinkWriter || !sample) {
        qCritical() << "Sink Writer or sample is null.";
        return false;
    }
    HRESULT status = sinkWriter->WriteSample(streamIndex, sample);
    if (FAILED(status)) {
        qCritical() << "Failed to write sample to Sink Writer.";
        return false;
    }

    return true;
}
bool finalizeSinkWriter(IMFSinkWriter* sinkWriter) {
    if (!sinkWriter) {
        qCritical() << "Sink Writer is null.";
        return false;
    }
    HRESULT status = sinkWriter->Finalize();
    if (FAILED(status)) {
        qCritical() << "Failed to finalize Sink Writer.";
        return false;
    }
    qDebug() << "Sink Writer finalized successfully.";
    return true;
}
void deleteSinkWriter(IMFSinkWriter* sinkWriter) {
    if (sinkWriter) {
        sinkWriter->Release();
        sinkWriter = nullptr;
        qDebug() << "Sink Writer deleted successfully.";
    }
}

// void recordFromAllCameras(const QString& basePath, int durationSeconds, UINT32 fps) {
//     initializeWMF();
//     IMFAttributes* videoAttributes = createCaptureAttributes(
//         MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
//     IMFAttributes* audioAttributes = createCaptureAttributes(
//         MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
//     UINT32 videoDeviceCount = 0, audioDeviceCount = 0;
//     IMFActivate** videoDevices = enumerateCaptureDevices(
//         videoAttributes, videoDeviceCount);
//     IMFActivate** audioDevices = enumerateCaptureDevices(
//         audioAttributes, audioDeviceCount);
//     if (!videoDevices) {
//         qCritical() << "No video devices found.";
//         deleteAttributes(videoAttributes);
//         deleteAttributes(audioAttributes);
//         deinitializeWMF();
//         return;
//     }
//     QDir dir(basePath);
//     if (!dir.exists()) {
//         if (!dir.mkpath(".")) {
//             qCritical() << "Failed to create directory:" << basePath;
//             deleteDeviceList(videoDevices, videoDeviceCount);
//             deleteDeviceList(audioDevices, audioDeviceCount);
//             deleteAttributes(videoAttributes);
//             deleteAttributes(audioAttributes);
//             deinitializeWMF();
//             return;
//         }
//     }
//     for (UINT32 i = 0; i < videoDeviceCount; ++i) {
//         IMFActivate* linkedAudioDevice = nullptr;

//         for (UINT32 j = 0; j < audioDeviceCount; ++j) {
//             if (areDevicesLinked(videoDevices[i], audioDevices[j])) {
//                 linkedAudioDevice = audioDevices[j];
//                 break;
//             }
//         }
//         QString outputPath = QString("%1/output_camera_%2.avi").arg(basePath).arg(i);
//         if (!recordFromDevice(videoDevices[i], linkedAudioDevice,
//                               outputPath, durationSeconds, fps)) {
//             qCritical() << "Failed to record from device:"
//                         << getDeviceName(videoDevices[i]);
//         }
//     }
//     deleteDeviceList(videoDevices, videoDeviceCount);
//     deleteDeviceList(audioDevices, audioDeviceCount);
//     deleteAttributes(videoAttributes);
//     deleteAttributes(audioAttributes);
//     deinitializeWMF();
// }

bool recordFromDevice(IMFActivate* videoDevice, IMFActivate* audioDevice,
                      const QString& outputPath, int durationSeconds, UINT32 fps) {
    IMFMediaSource* videoSource = createMediaSource(videoDevice);
    IMFMediaSource* audioSource = audioDevice ? createMediaSource(audioDevice) : nullptr;
    IMFSourceReader* videoReader = createSourceReader(videoSource);
    IMFSourceReader* audioReader = audioSource ? createSourceReader(audioSource) : nullptr;
    UINT32 videoWidth = 1920, videoHeight = 1080;
    UINT32 audioSampleRate = 48000, audioChannels = 2, audioBitsPerSample = 16;
    bool result = captureVideoWithAudio(
        videoReader, audioReader, outputPath,
        videoWidth, videoHeight, fps,
        audioSampleRate, audioChannels, audioBitsPerSample,
        durationSeconds);
    deleteSourceReader(videoReader);
    deleteSourceReader(audioReader);
    deleteMediaSource(videoSource);
    deleteMediaSource(audioSource);

    return result;
}

bool captureVideoWithAudio(IMFSourceReader* videoReader,
                           IMFSourceReader* audioReader, const QString& outputPath,
                           UINT32 videoWidth, UINT32 videoHeight, UINT32 videoFPS,
                           UINT32 audioSampleRate, UINT32 audioChannels, UINT32 audioBitsPerSample,
                           int durationSeconds) {
    IMFSinkWriter* sinkWriter = createSinkWriter(outputPath);
    if (!sinkWriter) {
        return false;
    }
    DWORD videoStreamIndex = 0, audioStreamIndex = 0;
    if (!configureOutputFormat(sinkWriter, videoStreamIndex,
                               videoWidth, videoHeight, videoFPS) ||
        !configureInputFormat(sinkWriter, videoStreamIndex,
                              videoWidth, videoHeight, videoFPS)) {
        deleteSinkWriter(sinkWriter);
        return false;
    }
    if (audioReader) {
        if (!configureAudioOutputFormat(sinkWriter, audioStreamIndex,
                                        audioSampleRate, audioChannels) ||
            !configureAudioInputFormat(sinkWriter, audioStreamIndex,
                                       audioSampleRate, audioChannels,
                                       audioBitsPerSample)) {
            deleteSinkWriter(sinkWriter);
            return false;
        }
    }
    if (!beginSinkWriter(sinkWriter)) {
        deleteSinkWriter(sinkWriter);
        return false;
    }
    if (!configureSourceReaderVideoFormat(videoReader,
                                          videoWidth, videoHeight, videoFPS)) {
        deleteSinkWriter(sinkWriter);
        return false;
    }
    if (audioReader) {
        if (!configureSourceReaderAudioFormat(audioReader,
                                              audioSampleRate, audioChannels,
                                              audioBitsPerSample)) {
            deleteSinkWriter(sinkWriter);
            return false;
        }
    }
    LONGLONG frameDuration = 10000000 / videoFPS;
    LONGLONG audioDuration = 0;
    int totalFrames = durationSeconds * videoFPS;
    LONGLONG rtStart = 0;
    for (int frameCount = 0; frameCount < totalFrames; ++frameCount) {
        auto frameStartTime = std::chrono::high_resolution_clock::now();

        IMFSample* videoSample = readSampleFromSourceReader(
            videoReader, MF_SOURCE_READER_FIRST_VIDEO_STREAM);
        if (videoSample) {
            videoSample->SetSampleTime(rtStart);
            videoSample->SetSampleDuration(frameDuration);

            if (!writeSample(sinkWriter, videoStreamIndex, videoSample)) {
                qCritical() << "Failed to write video sample.";
                videoSample->Release();
                break;
            }
            videoSample->Release();
        }
        if (audioReader) {
            IMFSample* audioSample = readSampleFromSourceReader(
                audioReader, MF_SOURCE_READER_FIRST_AUDIO_STREAM);
            if (audioSample) {
                audioSample->SetSampleTime(audioDuration);
                IMFMediaBuffer* buffer = nullptr;
                HRESULT hr = audioSample->ConvertToContiguousBuffer(&buffer);
                if (SUCCEEDED(hr)) {
                    DWORD cbBuffer = 0;
                    buffer->GetCurrentLength(&cbBuffer);
                    LONGLONG sampleDuration = (LONGLONG)((cbBuffer * 10000000) /
                                                          (audioSampleRate * audioChannels * (audioBitsPerSample / 8)));
                    audioSample->SetSampleDuration(sampleDuration);
                    audioDuration += sampleDuration;
                    buffer->Release();
                }
                if (!writeSample(sinkWriter, audioStreamIndex, audioSample)) {
                    qCritical() << "Failed to write audio sample.";
                    audioSample->Release();
                    break;
                }
                audioSample->Release();
            }
        }
        rtStart += frameDuration;
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            frameEndTime - frameStartTime);
        int frameDurationMs = 1000 / videoFPS;
        int remainingTime = frameDurationMs - static_cast<int>(elapsedTime.count());
        if (remainingTime > 0) {
            QThread::msleep(remainingTime);
        }
    }
    if (!finalizeSinkWriter(sinkWriter)) {
        deleteSinkWriter(sinkWriter);
        return false;
    }
    deleteSinkWriter(sinkWriter);
    qDebug() << "Capture complete. File saved to:" << outputPath;
    return true;
}

QList<QString> listDeviceInfo() {
    QList<QString> deviceInfoList;
    initializeWMF();
    IMFAttributes* videoAttributes = createCaptureAttributes(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    IMFAttributes* audioAttributes = createCaptureAttributes(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
    UINT32 videoDeviceCount = 0, audioDeviceCount = 0;
    IMFActivate** videoDevices = enumerateCaptureDevices(videoAttributes, videoDeviceCount);
    IMFActivate** audioDevices = enumerateCaptureDevices(audioAttributes, audioDeviceCount);
    if (videoDevices) {
        for (UINT32 i = 0; i < videoDeviceCount; ++i) {
            QString info;
            QString deviceName = getDeviceName(videoDevices[i]);
            QString deviceID = getDeviceID(videoDevices[i]);
            QString vendorID = getVendorID(videoDevices[i]);
            QString vendorName = getVendorNameFromId(vendorID);
            QString deviceProductName = getDeviceNameFromIds(vendorID, deviceID);

            QList<QString> videoCodecs = getAvailableVideoCodecs(videoDevices[i]);
            IMFActivate* linkedAudioDevice = nullptr;
            QList<QString> audioCodecs;
            for (UINT32 j = 0; j < audioDeviceCount; ++j) {
                if (areDevicesLinked(videoDevices[i], audioDevices[j])) {
                    linkedAudioDevice = audioDevices[j];
                    audioCodecs = getAvailableAudioCodecs(linkedAudioDevice);
                    break;
                }
            }
            info += QString("Index: %1\n").arg(i);
            info += QString("Name: %1\n").arg(deviceName);
            info += QString("  Vendor Name: %1\n").arg(vendorName.isEmpty() ? "Unknown" : vendorName);
            info += QString("  Device Name: %1\n").arg(deviceProductName.isEmpty() ? "Unknown" : deviceProductName);
            info += QString("  Vendor ID: %1\n").arg(vendorID);
            info += QString("  Device ID: %1\n").arg(deviceID);
            info += QString("  Available Video Codecs:\n");
            for (const QString& codecInfo : videoCodecs) {
                info += QString("    %1\n").arg(codecInfo);
            }
            if (linkedAudioDevice) {
                info += "  Linked Audio Device Found.\n";
                info += "  Available Audio Codecs:\n";
                for (const QString& codecInfo : audioCodecs) {
                    info += QString("    %1\n").arg(codecInfo);
                }
            } else {
                info += "  No linked audio device found for this camera.\n";
            }
            deviceInfoList.append(info);
        }
    }
    deleteDeviceList(videoDevices, videoDeviceCount);
    deleteDeviceList(audioDevices, audioDeviceCount);
    deleteAttributes(videoAttributes);
    deleteAttributes(audioAttributes);
    deinitializeWMF();
    return deviceInfoList;
}

bool loadUsbIds(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open usb.ids file:" << filePath;
        return false;
    }
    QTextStream in(&file);
    QString line;
    QString currentVendorId;
    QString currentVendorName;
    static const QRegularExpression whitespaceRegex("\\s+");
    while (!in.atEnd()) {
        line = in.readLine();
        if (line.trimmed().isEmpty() || line.trimmed().startsWith('#')) {
            continue;
        }
        int tabCount = 0;
        while (tabCount < line.length() && line[tabCount] == '\t') {
            tabCount++;
        }
        QString trimmedLine = line.mid(tabCount);
        trimmedLine = trimmedLine.trimmed();
        if (tabCount == 0) {
            QStringList parts = trimmedLine.split(whitespaceRegex, Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                currentVendorId = parts.takeFirst().toUpper();
                currentVendorName = parts.join(' ');
                vendorMap.insert(currentVendorId, currentVendorName);
            }
        } else if (tabCount == 1 && !currentVendorId.isEmpty()) {
            QStringList parts = trimmedLine.split(whitespaceRegex, Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                QString deviceId = parts.takeFirst().toUpper();
                QString deviceName = parts.join(' ');
                QPair<QString, QString> key(currentVendorId, deviceId);
                deviceMap.insert(key, deviceName);
            }
        }
    }
    file.close();
    return true;
}

QString getVendorNameFromId(const QString &vendorId) {
    return vendorMap.value(vendorId.toUpper(), QString());
}

QString getDeviceNameFromIds(const QString &vendorId, const QString &deviceId) {
    QPair<QString, QString> key(vendorId.toUpper(), deviceId.toUpper());
    return deviceMap.value(key, QString());
}

QString getAllCamerasInfo() {
    QString info;
    initializeWMF();
    IMFAttributes* videoAttributes = createCaptureAttributes(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    IMFAttributes* audioAttributes = createCaptureAttributes(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
    UINT32 videoDeviceCount = 0, audioDeviceCount = 0;
    IMFActivate** videoDevices = enumerateCaptureDevices(videoAttributes, videoDeviceCount);
    IMFActivate** audioDevices = enumerateCaptureDevices(audioAttributes, audioDeviceCount);
    if (videoDevices) {
        for (UINT32 i = 0; i < videoDeviceCount; ++i) {
            QString deviceInfo;
            QString deviceName = getDeviceName(videoDevices[i]);
            QString deviceID = getDeviceID(videoDevices[i]);
            QString vendorID = getVendorID(videoDevices[i]);
            QString vendorName = getVendorNameFromId(vendorID);
            QString deviceProductName = getDeviceNameFromIds(vendorID, deviceID);
            QList<QString> videoCodecs = getAvailableVideoCodecs(videoDevices[i]);
            IMFActivate* linkedAudioDevice = nullptr;
            QList<QString> audioCodecs;
            for (UINT32 j = 0; j < audioDeviceCount; ++j) {
                if (areDevicesLinked(videoDevices[i], audioDevices[j])) {
                    linkedAudioDevice = audioDevices[j];
                    audioCodecs = getAvailableAudioCodecs(linkedAudioDevice);
                    break;
                }
            }
            deviceInfo += QString("Camera %1:\n").arg(i);
            deviceInfo += QString("  Name: %1\n").arg(deviceName);
            deviceInfo += QString("  Vendor name: %1\n").arg(vendorName.isEmpty() ? "Unknown" : vendorName);
            deviceInfo += QString("  Device name: %1\n").arg(deviceProductName.isEmpty() ? "Unknown" : deviceProductName);
            deviceInfo += QString("  Vendor ID: %1\n").arg(vendorID);
            deviceInfo += QString("  Device ID: %1\n").arg(deviceID);
            deviceInfo += "  Available video codecs:\n";
            for (const QString& codecInfo : videoCodecs) {
                deviceInfo += QString("    %1\n").arg(codecInfo);
            }
            if (linkedAudioDevice) {
                deviceInfo += "  Associated audio device found.\n";
                deviceInfo += "  Available audio codecs:\n";
                for (const QString& codecInfo : audioCodecs) {
                    deviceInfo += QString("    %1\n").arg(codecInfo);
                }
            } else {
                deviceInfo += "  The associated audio device was not found\n";
            }
            deviceInfo += "\n";
            info += deviceInfo;
        }
    } else {
        info = "No cameras found.\n";
    }
    deleteDeviceList(videoDevices, videoDeviceCount);
    deleteDeviceList(audioDevices, audioDeviceCount);
    deleteAttributes(videoAttributes);
    deleteAttributes(audioAttributes);
    deinitializeWMF();
    return info;
}


QList<QString> recordVideoWithAudioFromAllCameras(const QString& basePath, int durationSeconds, UINT32 fps) {
    QList<QString> videoPaths;
    initializeWMF();
    IMFAttributes* videoAttributes = createCaptureAttributes(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    IMFAttributes* audioAttributes = createCaptureAttributes(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
    UINT32 videoDeviceCount = 0, audioDeviceCount = 0;
    IMFActivate** videoDevices = enumerateCaptureDevices(videoAttributes, videoDeviceCount);
    IMFActivate** audioDevices = enumerateCaptureDevices(audioAttributes, audioDeviceCount);
    if (!videoDevices) {
        qCritical() << "No cameras found.";
        deleteAttributes(videoAttributes);
        deleteAttributes(audioAttributes);
        deinitializeWMF();
        return videoPaths;
    }
    QDir dir(basePath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qCritical() << "Failed to create directory:" << basePath;
            deleteDeviceList(videoDevices, videoDeviceCount);
            deleteDeviceList(audioDevices, audioDeviceCount);
            deleteAttributes(videoAttributes);
            deleteAttributes(audioAttributes);
            deinitializeWMF();
            return videoPaths;
        }
    }
    for (UINT32 i = 0; i < videoDeviceCount; ++i) {
        IMFActivate* linkedAudioDevice = nullptr;

        for (UINT32 j = 0; j < audioDeviceCount; ++j) {
            if (areDevicesLinked(videoDevices[i], audioDevices[j])) {
                linkedAudioDevice = audioDevices[j];
                break;
            }
        }
        QString outputPath = QString("%1/video_camera_%2.avi").arg(basePath).arg(i);
        if (!recordFromDevice(videoDevices[i], linkedAudioDevice, outputPath, durationSeconds, fps)) {
            qCritical() << "Failed to record video from device:" << getDeviceName(videoDevices[i]);
        } else {
            videoPaths.append(outputPath);
        }
    }
    deleteDeviceList(videoDevices, videoDeviceCount);
    deleteDeviceList(audioDevices, audioDeviceCount);
    deleteAttributes(videoAttributes);
    deleteAttributes(audioAttributes);
    deinitializeWMF();
    return videoPaths;
}

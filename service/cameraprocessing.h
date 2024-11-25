#pragma once

#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <comdef.h>
#include <QString>
#include <QMap>
#include <QList>
#include <QPair>
#include <QMap>

void initializeWMF();
void deinitializeWMF();

IMFAttributes* createCaptureAttributes(const GUID& sourceType);
void deleteAttributes(IMFAttributes* attributes);

IMFActivate** enumerateCaptureDevices(IMFAttributes* attributes, UINT32& deviceCount);
void deleteDeviceList(IMFActivate** devices, UINT32 deviceCount);

QString getDeviceName(IMFActivate* device);
bool areDevicesLinked(IMFActivate* videoDevice, IMFActivate* audioDevice);

IMFMediaSource* createMediaSource(IMFActivate* device);
void deleteMediaSource(IMFMediaSource* mediaSource);

IMFSourceReader* createSourceReader(IMFMediaSource* mediaSource);
void deleteSourceReader(IMFSourceReader* sourceReader);

IMFSample* readSampleFromSourceReader(IMFSourceReader* sourceReader, DWORD streamType, int maxAttempts = 10);

bool configureSourceReaderVideoFormat(IMFSourceReader* sourceReader, UINT32 width, UINT32 height, UINT32 fps);
bool configureSourceReaderAudioFormat(IMFSourceReader* sourceReader, UINT32 sampleRate, UINT32 channels, UINT32 bitsPerSample);

IMFSinkWriter* createSinkWriter(const QString& outputPath);
void deleteSinkWriter(IMFSinkWriter* sinkWriter);

bool configureOutputFormat(IMFSinkWriter* sinkWriter, DWORD& streamIndex, UINT32 width, UINT32 height, UINT32 fps);
bool configureInputFormat(IMFSinkWriter* sinkWriter, DWORD streamIndex, UINT32 width, UINT32 height, UINT32 fps);
bool configureAudioOutputFormat(IMFSinkWriter* sinkWriter, DWORD& audioStreamIndex, UINT32 sampleRate, UINT32 channels);
bool configureAudioInputFormat(IMFSinkWriter* sinkWriter, DWORD audioStreamIndex, UINT32 sampleRate, UINT32 channels, UINT32 bitsPerSample);

bool beginSinkWriter(IMFSinkWriter* sinkWriter);
bool writeSample(IMFSinkWriter* sinkWriter, DWORD streamIndex, IMFSample* sample);
bool finalizeSinkWriter(IMFSinkWriter* sinkWriter);

//void recordFromAllCameras(const QString& basePath, int durationSeconds, UINT32 fps);
bool captureVideoWithAudio(IMFSourceReader* videoReader, IMFSourceReader* audioReader, const QString& outputPath,
                           UINT32 videoWidth, UINT32 videoHeight, UINT32 videoFPS,
                           UINT32 audioSampleRate, UINT32 audioChannels, UINT32 audioBitsPerSample,
                           int durationSeconds);
bool recordFromDevice(IMFActivate* videoDevice, IMFActivate* audioDevice,
                      const QString& outputPath, int durationSeconds, UINT32 fps);


QString getDeviceSymbolicLink(IMFActivate* device);
QString getDeviceID(IMFActivate* device);
QString getVendorID(IMFActivate* device);
QList<QString> getAvailableVideoCodecs(IMFActivate* videoDevice);
QList<QString> getAvailableAudioCodecs(IMFActivate* audioDevice);
bool loadUsbIds(const QString& filePath);
QString getVendorNameFromId(const QString& vendorId);
QString getDeviceNameFromIds(const QString& vendorId, const QString& deviceId);

// void listDeviceInfo();
QList<QString> listDeviceInfo();

QString getAllCamerasInfo();
QList<QString> recordVideoWithAudioFromAllCameras(const QString& basePath, int durationSeconds, UINT32 fps);


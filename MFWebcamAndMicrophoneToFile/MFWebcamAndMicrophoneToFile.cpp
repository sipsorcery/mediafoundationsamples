/******************************************************************************
* Filename: MFWebcamAndMicrophoneToFile.cpp
*
* Description:
* This file contains a C++ console application that captures the real-time video
* stream from a webcam as well as the output from an audio capture device to an
* MP4 file.
*
* Note: The webcam/audio device indexes and the source reader media output types
* will need adjustment depending on the configuration of video devices on the
* machine running this sample.
*
* Status: Work in progress (sink writer stalling)
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 30 Jan 2020		Aaron Clauson	Created, Hobart, Australia.
*
* License: Public Domain (no warranty, use at own risk)
/******************************************************************************/

#include "../Common/MFUtility.h"

#include <stdio.h>
#include <tchar.h>
#include <evr.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include <codecapi.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

#define WEBCAM_DEVICE_INDEX 0					// Adjust according to desired video capture device.
#define AUDIO_CAPTURE_DEVICE_INDEX 0	// Adjust according to desired audio capture device.
#define SAMPLE_COUNT 200							// Adjust depending on number of samples to capture.
#define CAPTURE_FILENAME L"capture.mp4"
#define OUTPUT_FRAME_WIDTH 640				// Adjust if the webcam does not support this frame width.
#define OUTPUT_FRAME_HEIGHT 480				// Adjust if the webcam does not support this frame height.
#define OUTPUT_FRAME_RATE 30					// Adjust if the webcam does not support this frame rate.

int main()
{
  IMFMediaSource* pVideoSource = NULL, * pAudioSource = NULL;
  IMFMediaSource* pAggSource = NULL;
  IMFCollection* pCollection = NULL;
  UINT32 videoDeviceCount = 0;
  IMFAttributes* videoConfig = NULL;
  IMFSourceReader* pSourceReader = NULL;
  WCHAR* webcamFriendlyName;
  IMFMediaType* pVideoSrcOut = NULL, * pAudioSrcOut = NULL;
  IMFSinkWriter* pWriter = NULL;
  IMFMediaType* pVideoSinkOut = NULL, * pAudioSinkOut = NULL;
  DWORD srcVideoStreamIndex = 0, srcAudioStreamIndex = 0;
  DWORD sinkVideoStreamIndex = 0, sinkAudioStreamIndex = 0;

  CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
    "COM initialisation failed.");

  CHECK_HR(MFStartup(MF_VERSION),
    "Media Foundation initialisation failed.");

  // Get the sources for the video and audio capture devices.

  CHECK_HR(GetSourceFromCaptureDevice(DeviceType::Video, WEBCAM_DEVICE_INDEX, &pVideoSource, nullptr),
    "Failed to get video source and reader.");

  CHECK_HR(GetSourceFromCaptureDevice(DeviceType::Audio, AUDIO_CAPTURE_DEVICE_INDEX, &pAudioSource, nullptr),
    "Failed to get video source and reader.");

  // Combine the two into an aggregate source and create a reader.

  CHECK_HR(MFCreateCollection(&pCollection), "Failed to create source collection.");
  CHECK_HR(pCollection->AddElement(pVideoSource), "Failed to add video source to collection.");
  CHECK_HR(pCollection->AddElement(pAudioSource), "Failed to add audio source to collection.");

  CHECK_HR(MFCreateAggregateSource(pCollection, &pAggSource),
    "Failed to create aggregate source.");

  CHECK_HR(MFCreateSourceReaderFromMediaSource(pAggSource, NULL, &pSourceReader),
    "Error creating media source reader.");

  // Note the webcam needs to support this media type. 
  CHECK_HR(MFCreateMediaType(&pVideoSrcOut), "Failed to create media type.");
  CHECK_HR(pVideoSrcOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set major video type.");
  CHECK_HR(pVideoSrcOut->SetGUID(MF_MT_SUBTYPE, WMMEDIASUBTYPE_I420), "Failed to set video sub type to I420.");
  CHECK_HR(MFSetAttributeRatio(pVideoSrcOut, MF_MT_FRAME_RATE, OUTPUT_FRAME_RATE, 1), "Failed to set frame rate on source reader out type.");
  CHECK_HR(MFSetAttributeSize(pVideoSrcOut, MF_MT_FRAME_SIZE, OUTPUT_FRAME_WIDTH, OUTPUT_FRAME_HEIGHT), "Failed to set frame size.");

  CHECK_HR(pSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pVideoSrcOut),
    "Failed to set video media type on source reader.");

  CHECK_HR(MFCreateMediaType(&pAudioSrcOut), "Failed to create media type.");
  CHECK_HR(pAudioSrcOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio), "Failed to set major audio type.");
  CHECK_HR(pAudioSrcOut->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float), "Failed to set audio sub type.");

  CHECK_HR(pSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, pAudioSrcOut),
    "Failed to set audio media type on source reader.");

  DWORD stmIndex = 0;
  BOOL isSelected = false;
  IMFMediaType* pStmMediaType = NULL;
  while (pSourceReader->GetStreamSelection(stmIndex, &isSelected) == S_OK) {
    printf("Stream %d is selected %d.\n", stmIndex, isSelected);

    CHECK_HR(pSourceReader->GetCurrentMediaType(stmIndex, &pStmMediaType), "Failed to get media type for selected stream.");

    std::cout << "Media type: " << GetMediaTypeDescription(pStmMediaType) << std::endl;

    GUID majorMediaType;
    pStmMediaType->GetGUID(MF_MT_MAJOR_TYPE, &majorMediaType);
    if (majorMediaType == MFMediaType_Audio) {
      std::cout << "Source audio stream index is " << stmIndex << "." << std::endl;
      srcAudioStreamIndex = stmIndex;
    }
    else if (majorMediaType == MFMediaType_Video) {
      std::cout << "Video stream index is " << stmIndex << "." << std::endl;
      srcVideoStreamIndex = stmIndex;
    }

    stmIndex++;
    SAFE_RELEASE(pStmMediaType);
  }

  // Create the MP4 sink writer and add the audio and video streams.

  CHECK_HR(MFCreateSinkWriterFromURL(
    CAPTURE_FILENAME,
    NULL,
    NULL,
    &pWriter),
    "Error creating mp4 sink writer.");

  // Configure the output video type on the sink writer.
  CHECK_HR(MFCreateMediaType(&pVideoSinkOut), "Configure encoder failed to create media type for video output sink.");
  CHECK_HR(pVideoSrcOut->CopyAllItems(pVideoSinkOut), "Error copying media type attributes from video source output media type.");
  CHECK_HR(pVideoSinkOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264), "Failed to set video writer attribute, video format (H.264).");
  CHECK_HR(pVideoSinkOut->SetUINT32(MF_MT_AVG_BITRATE, 240000), "Error setting average bit rate.");
  CHECK_HR(pVideoSinkOut->SetUINT32(MF_MT_INTERLACE_MODE, 2), "Error setting interlace mode.");
  CHECK_HR(MFSetAttributeRatio(pVideoSinkOut, MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Base, 1), "Failed to set profile on H264 MFT out type.");

  CHECK_HR(pWriter->AddStream(pVideoSinkOut, &sinkVideoStreamIndex),
    "Failed to add the video stream to the sink writer.");

  CHECK_HR(pWriter->SetInputMediaType(sinkVideoStreamIndex, pVideoSrcOut, NULL),
    "Error setting the sink writer video input type.");

  // Configure the audio type on the sink writer.
  CHECK_HR(MFCreateMediaType(&pAudioSinkOut), "Configure encoder failed to create media type for audio output sink.");
  CHECK_HR(pAudioSrcOut->CopyAllItems(pAudioSinkOut), "Error copying media type attributes from audio source output media type.");

  CHECK_HR(pWriter->AddStream(pAudioSinkOut, &sinkAudioStreamIndex),
    "Failed to add the audio stream to the sink writer.");

  CHECK_HR(pWriter->SetInputMediaType(sinkAudioStreamIndex, pAudioSrcOut, NULL),
    "Error setting the sink writer video input type.");

  CHECK_HR(pWriter->BeginWriting(),
    "Failed to begin writing on the MP4 sink.");

  DWORD streamIndex, flags;
  LONGLONG llSampleTimeStamp;
  IMFSample* pSample = NULL;
  LONGLONG llSampleBaseTime = 0;
  int sampleCount = 0;

  printf("Recording...\n");

  while (sampleCount < SAMPLE_COUNT)
  {
    CHECK_HR(pSourceReader->ReadSample(
      MF_SOURCE_READER_ANY_STREAM,
      0,																	// Flags.
      &streamIndex,												// Receives the actual stream index. 
      &flags,															// Receives status flags.
      &llSampleTimeStamp,									// Receives the time stamp.
      &pSample												// Receives the sample or NULL.
    ), "Error reading sample.");

    if (llSampleBaseTime == 0) {
      llSampleBaseTime = llSampleTimeStamp;
    }

    DWORD sinkStmIndex = (streamIndex == srcAudioStreamIndex) ? sinkAudioStreamIndex : sinkVideoStreamIndex;
    // Re-base the time stamp.
    llSampleTimeStamp -= llSampleBaseTime;

   /* if (flags & MF_SOURCE_READERF_STREAMTICK) {
      printf("Stream tick sink stream index %d.\n", sinkStmIndex);
      pWriter->SendStreamTick(sinkStmIndex, llSampleTimeStamp);
    }*/

    if (pSample) {
      printf("sample %d, source stream index %d, sink stream index %d, timestamp %I64d.\n", sampleCount, streamIndex, sinkStmIndex, llSampleTimeStamp);

      CHECK_HR(pSample->SetSampleTime(llSampleTimeStamp), "Set sample time failed.");
      CHECK_HR(pWriter->WriteSample(sinkStmIndex, pSample), "Write sample failed.");
    }

    SAFE_RELEASE(&pSample);

    sampleCount++;
  }

  printf("Finalising the capture.\n");

  CHECK_HR(pWriter->Finalize(), "Error finalising MP4 sink writer.");

done:

  printf("finished.\n");
  auto c = getchar();

  SAFE_RELEASE(pAggSource);
  SAFE_RELEASE(pVideoSource);
  SAFE_RELEASE(pAudioSource);
  SAFE_RELEASE(videoConfig);
  SAFE_RELEASE(pSourceReader);
  SAFE_RELEASE(pVideoSrcOut);
  SAFE_RELEASE(pAudioSrcOut);
  SAFE_RELEASE(pVideoSinkOut);
  SAFE_RELEASE(pAudioSinkOut);
  SAFE_RELEASE(pWriter);

  return 0;
}

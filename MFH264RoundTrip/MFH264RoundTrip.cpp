/******************************************************************************
* Filename: MFH264RoundTrip.cpp
*
* Description:
* This file contains a C++ console application that captures the real-time video
* stream from a webcam to an H264 byte array using the H264 encoder Media Foundation
* Transform (MFT) and then uses the reverse H264 decoder MFT transform to get back
* the raw image frames.
*
* To convert the raw yuv data dumped at the end of this sample use the ffmpeg command below:
* ffmpeg -vcodec rawvideo -s 640x480 -pix_fmt yuv420p -i rawframes.yuv -vframes 1 output.jpeg
* ffmpeg -vcodec rawvideo -s 640x480 -pix_fmt yuv420p -i rawframes.yuv out.avi
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 05 Mar 2015	  Aaron Clauson	  Created, Hobart, Australia.
*
* License: Public Domain (no warranty, use at own risk)
/******************************************************************************/

#include "../Common/MFUtility.h"

#include <stdio.h>
#include <tchar.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include <codecapi.h>

#include <fstream>
#include <iostream>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

#define WEBCAM_DEVICE_INDEX 0	    // Adjust according to desired video capture device.
#define SAMPLE_COUNT 1000			    // Adjust depending on number of samples to capture.
#define OUTPUT_FRAME_WIDTH 640		// Adjust if the webcam does not support this frame width.
#define OUTPUT_FRAME_HEIGHT 480		// Adjust if the webcam does not support this frame height.
#define OUTPUT_FRAME_RATE 30      // Adjust if the webcam does not support this frame rate.
#define CAPTURE_FILENAME "rawframes.yuv"

int _tmain(int argc, _TCHAR* argv[])
{
  std::ofstream outputBuffer(CAPTURE_FILENAME, std::ios::out | std::ios::binary);

  IMFMediaSource* pVideoSource = NULL;
  IMFSourceReader* pVideoReader = NULL;
  WCHAR* webcamFriendlyName;
  IMFMediaType* pSrcOutMediaType = NULL;
  IUnknown* spEncoderTransfromUnk = NULL;
  IMFTransform* pEncoderTransfrom = NULL; // This is H264 Encoder MFT.
  IMFMediaType* pMFTInputMediaType = NULL, * pMFTOutputMediaType = NULL;
  UINT friendlyNameLength = 0;
  IUnknown* spDecTransformUnk = NULL;
  IMFTransform* pDecoderTransform = NULL; // This is H264 Decoder MFT.
  IMFMediaType* pDecInputMediaType = NULL, * pDecOutputMediaType = NULL;
  DWORD mftStatus = 0;

  CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
    "COM initialisation failed.");

  CHECK_HR(MFStartup(MF_VERSION),
    "Media Foundation initialisation failed.");

  // Get video capture device.
  CHECK_HR(GetVideoSourceFromDevice(WEBCAM_DEVICE_INDEX, &pVideoSource, &pVideoReader),
    "Failed to get webcam video source.");

  // Note the webcam needs to support this media type. 
  // The list of media types supported can be obtained using the ListTypes function in MFUtility.h.
  MFCreateMediaType(&pSrcOutMediaType);
  CHECK_HR(pSrcOutMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set major video type.");
  CHECK_HR(pSrcOutMediaType->SetGUID(MF_MT_SUBTYPE, WMMEDIASUBTYPE_I420), "Failed to set video sub type to I420.");
  CHECK_HR(MFSetAttributeRatio(pSrcOutMediaType, MF_MT_FRAME_RATE, OUTPUT_FRAME_RATE, 1), "Failed to set frame rate on source reader out type.");
  CHECK_HR(MFSetAttributeSize(pSrcOutMediaType, MF_MT_FRAME_SIZE, OUTPUT_FRAME_WIDTH, OUTPUT_FRAME_HEIGHT), "Failed to set frame size.");

  CHECK_HR(pVideoReader->SetCurrentMediaType(0, NULL, pSrcOutMediaType),
    "Failed to set media type on source reader.");

  printf("%s\n", GetMediaTypeDescription(pSrcOutMediaType).c_str());

  //CHECK_HR(MFTRegisterLocalByCLSID(
  //  __uuidof(CColorConvertDMO),
  //  MFT_CATEGORY_VIDEO_PROCESSOR,
  //  L"",
  //  MFT_ENUM_FLAG_SYNCMFT,
  //  0,
  //  NULL,
  //  0,
  //  NULL
  //), "Error registering colour converter DSP.\n");

  // Create H.264 encoder.
  CHECK_HR(CoCreateInstance(CLSID_CMSH264EncoderMFT, NULL, CLSCTX_INPROC_SERVER,
    IID_IUnknown, (void**)&spEncoderTransfromUnk),
    "Failed to create H264 encoder MFT.");

  CHECK_HR(spEncoderTransfromUnk->QueryInterface(IID_PPV_ARGS(&pEncoderTransfrom)),
    "Failed to get IMFTransform interface from H264 encoder MFT object.");

  MFCreateMediaType(&pMFTInputMediaType);
  CHECK_HR(pSrcOutMediaType->CopyAllItems(pMFTInputMediaType), "Error copying media type attributes to decoder output media type.");
  CHECK_HR(pMFTInputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV), "Error setting video subtype.");

  MFCreateMediaType(&pMFTOutputMediaType);
  CHECK_HR(pMFTInputMediaType->CopyAllItems(pMFTOutputMediaType), "Error copying media type attributes tfrom mft input type to mft output type.");
  CHECK_HR(pMFTOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264), "Error setting video sub type.");
  CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_MT_AVG_BITRATE, 240000), "Error setting average bit rate.");
  CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2), "Error setting interlace mode.");
  CHECK_HR(MFSetAttributeRatio(pMFTOutputMediaType, MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Base, 1), "Failed to set profile on H264 MFT out type.");
  //CHECK_HR(pMFTOutputMediaType->SetDouble(MF_MT_MPEG2_LEVEL, 3.1), "Failed to set level on H264 MFT out type.\n");
  //CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_MT_MAX_KEYFRAME_SPACING, 10), "Failed to set key frame interval on H264 MFT out type.\n");
  //CHECK_HR(pMFTOutputMediaType->SetUINT32(CODECAPI_AVEncCommonQuality, 100), "Failed to set H264 codec qulaity.\n");

  std::cout << "H264 encoder output type: " << GetMediaTypeDescription(pMFTOutputMediaType) << std::endl;

  CHECK_HR(pEncoderTransfrom->SetOutputType(0, pMFTOutputMediaType, 0),
    "Failed to set output media type on H.264 encoder MFT.");

  std::cout << "H264 encoder input type: " << GetMediaTypeDescription(pMFTInputMediaType) << std::endl;

  CHECK_HR(pEncoderTransfrom->SetInputType(0, pMFTInputMediaType, 0),
    "Failed to set input media type on H.264 encoder MFT.");

  CHECK_HR(pEncoderTransfrom->GetInputStatus(0, &mftStatus), "Failed to get input status from H.264 MFT.");
  if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
    printf("E: ApplyTransform() pEncoderTransfrom->GetInputStatus() not accept data.\n");
    goto done;
  }

  //CHECK_HR(pEncoderTransfrom->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL), "Failed to process FLUSH command on H.264 MFT.");
  CHECK_HR(pEncoderTransfrom->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), "Failed to process BEGIN_STREAMING command on H.264 MFT.");
  CHECK_HR(pEncoderTransfrom->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), "Failed to process START_OF_STREAM command on H.264 MFT.");

  // Create H.264 decoder.
  CHECK_HR(CoCreateInstance(CLSID_CMSH264DecoderMFT, NULL, CLSCTX_INPROC_SERVER,
    IID_IUnknown, (void**)&spDecTransformUnk), "Failed to create H264 decoder MFT.");

  CHECK_HR(spDecTransformUnk->QueryInterface(IID_PPV_ARGS(&pDecoderTransform)),
    "Failed to get IMFTransform interface from H264 decoder MFT object.");

  MFCreateMediaType(&pDecInputMediaType);
  CHECK_HR(pMFTOutputMediaType->CopyAllItems(pDecInputMediaType), "Error copying media type attributes to decoder input media type.");
  CHECK_HR(pDecoderTransform->SetInputType(0, pDecInputMediaType, 0), "Failed to set input media type on H.264 decoder MFT.");

  MFCreateMediaType(&pDecOutputMediaType);
  CHECK_HR(pMFTInputMediaType->CopyAllItems(pDecOutputMediaType), "Error copying media type attributes to decoder output media type.");
  CHECK_HR(pDecoderTransform->SetOutputType(0, pDecOutputMediaType, 0), "Failed to set output media type on H.264 decoder MFT.");

  CHECK_HR(pDecoderTransform->GetInputStatus(0, &mftStatus), "Failed to get input status from H.264 decoder MFT.");
  if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
    printf("H.264 decoder MFT is not accepting data.\n");
    goto done;
  }

  CHECK_HR(pDecoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL), "Failed to process FLUSH command on H.264 decoder MFT.");
  CHECK_HR(pDecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), "Failed to process BEGIN_STREAMING command on H.264 decoder MFT.");
  CHECK_HR(pDecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), "Failed to process START_OF_STREAM command on H.264 decoder MFT.");

  // Ready to go.

  printf("Reading video samples from webcam.\n");

  IMFSample* pVideoSample = NULL, * pH264EncodeOutSample = NULL, * pH264DecodeOutSample = NULL;
  DWORD streamIndex = 0, flags = 0, sampleFlags = 0;
  LONGLONG llVideoTimeStamp, llSampleDuration;
  int sampleCount = 0;
  BOOL h264EncodeTransformFlushed = FALSE;
  BOOL h264DecodeTransformFlushed = FALSE;

  while (sampleCount <= SAMPLE_COUNT)
  {
    CHECK_HR(pVideoReader->ReadSample(
      MF_SOURCE_READER_FIRST_VIDEO_STREAM,
      0,                              // Flags.
      &streamIndex,                   // Receives the actual stream index. 
      &flags,                         // Receives status flags.
      &llVideoTimeStamp,              // Receives the time stamp.
      &pVideoSample                    // Receives the sample or NULL.
    ), "Error reading video sample.");

    if (flags & MF_SOURCE_READERF_STREAMTICK)
    {
      printf("\tStream tick.\n");
    }
    if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
    {
      printf("\tEnd of stream.\n");
      break;
    }
    if (flags & MF_SOURCE_READERF_NEWSTREAM)
    {
      printf("\tNew stream.\n");
      break;
    }
    if (flags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)
    {
      printf("\tNative type changed.\n");
      break;
    }
    if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
    {
      printf("\tCurrent type changed.\n");
      break;
    }

    if (pVideoSample)
    {
      printf("Sample %i.\n", sampleCount);

      CHECK_HR(pVideoSample->SetSampleTime(llVideoTimeStamp), "Error setting the video sample time.");
      CHECK_HR(pVideoSample->GetSampleDuration(&llSampleDuration), "Error getting video sample duration.");
      CHECK_HR(pVideoSample->GetSampleFlags(&sampleFlags), "Error getting sample flags.");

      printf("Sample count %d, Sample flags %d, sample duration %I64d, sample time %I64d\n", sampleCount, sampleFlags, llSampleDuration, llVideoTimeStamp);

      // Apply the H264 encoder transform
      CHECK_HR(pEncoderTransfrom->ProcessInput(0, pVideoSample, 0),
        "The H264 encoder ProcessInput call failed.");

      // ***** H264 ENcoder transform processing loop. *****

      HRESULT getEncoderResult = S_OK;
      while (getEncoderResult == S_OK) {

        getEncoderResult = GetTransformOutput(pEncoderTransfrom, &pH264EncodeOutSample, &h264EncodeTransformFlushed);

        if (getEncoderResult != S_OK && getEncoderResult != MF_E_TRANSFORM_NEED_MORE_INPUT) {
          printf("Error getting H264 encoder transform output, error code %.2X.\n", getEncoderResult);
          goto done;
        }

        if (h264EncodeTransformFlushed == TRUE) {
          // H264 encoder format changed. Clear the capture file and start again.
          printf("H264 encoder transform flushed stream.\n");
        }
        else if (pH264EncodeOutSample != NULL) {
          printf("Applying decoder transform.\n");

          // Apply the H264 decoder transform
          CHECK_HR(pDecoderTransform->ProcessInput(0, pH264EncodeOutSample, 0),
            "The H264 decoder ProcessInput call failed.");

          // ----- H264 DEcoder transform processing loop. -----

          HRESULT getDecoderResult = S_OK;
          while (getDecoderResult == S_OK) {

            // Apply the H264 decoder transform
            getDecoderResult = GetTransformOutput(pDecoderTransform, &pH264DecodeOutSample, &h264DecodeTransformFlushed);

            if (getDecoderResult != S_OK && getDecoderResult != MF_E_TRANSFORM_NEED_MORE_INPUT) {
              printf("Error getting H264 decoder transform output, error code %.2X.\n", getDecoderResult);
              goto done;
            }

            if (h264DecodeTransformFlushed == TRUE) {
              // H264 decoder format changed. Clear the capture file and start again.
              printf("H264 decoder transform flushed stream.\n");
              outputBuffer.close();
              outputBuffer.open(CAPTURE_FILENAME, std::ios::out | std::ios::binary);
            }
            else if (pH264DecodeOutSample != NULL) {
              // Write decoded sample to capture file.
              CHECK_HR(WriteSampleToFile(pH264DecodeOutSample, &outputBuffer),
                "Failed to write sample to file.");
            }

            SAFE_RELEASE(pH264DecodeOutSample);
          }
          // -----

        }

        SAFE_RELEASE(pH264EncodeOutSample);
      }
      // *****

      sampleCount++;

      // Note: Apart from memory leak issues if the media samples are not released the videoReader->ReadSample
      // blocks when it is unable to allocate a new sample.
      SAFE_RELEASE(pVideoSample);
      SAFE_RELEASE(pH264EncodeOutSample);
      SAFE_RELEASE(pH264DecodeOutSample);
    }
  }

done:

  outputBuffer.close();

  printf("finished.\n");
  auto c = getchar();

  SAFE_RELEASE(pVideoSource);
  SAFE_RELEASE(pVideoReader);
  SAFE_RELEASE(pSrcOutMediaType);
  SAFE_RELEASE(spEncoderTransfromUnk);
  SAFE_RELEASE(pEncoderTransfrom);
  SAFE_RELEASE(pMFTInputMediaType);
  SAFE_RELEASE(pMFTOutputMediaType);
  SAFE_RELEASE(spDecTransformUnk);
  SAFE_RELEASE(pDecoderTransform);
  SAFE_RELEASE(pDecInputMediaType);
  SAFE_RELEASE(pDecOutputMediaType);

  return 0;
}
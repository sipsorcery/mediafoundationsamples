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
* Status: Not Working.
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

#include <fstream>
#include <iostream>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

#define WEBCAM_DEVICE_INDEX 0	// Adjust according to desired video capture device.
#define SAMPLE_COUNT 100			// Adjust depending on number of samples to capture.
#define CAPTURE_FILENAME "rawframes.yuv"

int _tmain(int argc, _TCHAR* argv[])
{
  std::ofstream outputBuffer(CAPTURE_FILENAME, std::ios::out | std::ios::binary);

  IMFMediaSource* videoSource = NULL;
  UINT32 videoDeviceCount = 0;
  IMFAttributes* videoConfig = NULL;
  IMFActivate** videoDevices = NULL;
  IMFSourceReader* videoReader = NULL;
  WCHAR* webcamFriendlyName;
  IMFMediaType* videoSourceOutputType = NULL, * pSrcOutMediaType = NULL;
  IUnknown* spEncoderTransfromUnk = NULL;
  IMFTransform* pEncoderTransfrom = NULL; // This is H264 Encoder MFT.
  IWMResamplerProps* spResamplerProps = NULL;
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

  // Get the first available webcam.
  CHECK_HR(MFCreateAttributes(&videoConfig, 1),
    "Error creating video configuation.");

  // Request video capture devices.
  CHECK_HR(videoConfig->SetGUID(
    MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
    MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID),
    "Error initialising video configuration object.");

  CHECK_HR(MFEnumDeviceSources(videoConfig, &videoDevices, &videoDeviceCount),
    "Error enumerating video devices.");

  CHECK_HR(videoDevices[WEBCAM_DEVICE_INDEX]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &webcamFriendlyName, &friendlyNameLength),
    "Error retrieving vide device friendly name.");

  wprintf(L"First available webcam: %s\n", webcamFriendlyName);

  CHECK_HR(videoDevices[WEBCAM_DEVICE_INDEX]->ActivateObject(IID_PPV_ARGS(&videoSource)),
    "Error activating video device.");

  // Create a source reader.
  CHECK_HR(MFCreateSourceReaderFromMediaSource(
    videoSource,
    videoConfig,
    &videoReader),
    "Error creating video source reader.");

  CHECK_HR(videoReader->GetCurrentMediaType(
    (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
    &videoSourceOutputType),
    "Error retrieving current media type from first video stream.");

  // Note the webcam needs to support this media type. 
  // The list of media types supported can be obtained using the ListTypes function in MFUtility.h.
  MFCreateMediaType(&pSrcOutMediaType);
  pSrcOutMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  pSrcOutMediaType->SetGUID(MF_MT_SUBTYPE, WMMEDIASUBTYPE_I420);
  MFSetAttributeSize(pSrcOutMediaType, MF_MT_FRAME_SIZE, 640, 480);

  CHECK_HR(videoReader->SetCurrentMediaType(0, NULL, pSrcOutMediaType),
    "Failed to set media type on source reader.");

  CHECK_HR(videoReader->GetCurrentMediaType(
    (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
    &videoSourceOutputType),
    "Error retrieving current media type from first video stream.");

  std::cout << GetMediaTypeDescription(videoSourceOutputType) << std::endl;

  CHECK_HR(MFTRegisterLocalByCLSID(
    __uuidof(CColorConvertDMO),
    MFT_CATEGORY_VIDEO_PROCESSOR,
    L"",
    MFT_ENUM_FLAG_SYNCMFT,
    0,
    NULL,
    0,
    NULL
  ), "Error registering colour converter DSP.\n");

  // Create H.264 encoder.
  // Note that https://docs.microsoft.com/en-us/windows/win32/medfound/media-type-negotiation-on-the-encoder 
  // specifies a transform input type must be set before the output type.
  CHECK_HR(CoCreateInstance(CLSID_CMSH264EncoderMFT, NULL, CLSCTX_INPROC_SERVER,
    IID_IUnknown, (void**)&spEncoderTransfromUnk),
    "Failed to create H264 encoder MFT.");

  CHECK_HR(spEncoderTransfromUnk->QueryInterface(IID_PPV_ARGS(&pEncoderTransfrom)),
    "Failed to get IMFTransform interface from H264 encoder MFT object.");

  //8912, 1C58 09:21 : 41.26475 CMFTransformDetours::SetOutputType @001FC134 Succeeded MT : MF_MT_MAJOR_TYPE = MEDIATYPE_Video; MF_MT_SUBTYPE = MEDIASUBTYPE_H264; MF_MT_AVG_BITRATE = 240000; MF_MT_FRAME_SIZE = 2748779069920 (640, 480); MF_MT_FRAME_RATE = 128849018881 (30, 1); MF_MT_PIXEL_ASPECT_RATIO = 4294967297 (1, 1); MF_MT_INTERLACE_MODE = 2; MF_MT_MPEG2_PROFILE = 66; MF_MT_MPEG_SEQUENCE_HEADER = 00 00 00 01 67 42 c0 1e 96 54 05 01 e9 80 80 40 00 00 00 01 68 ce 3c 80
  //8912, 1C58 09:21 : 41.26479 CMFTransformDetours::SetInputType @001FC134 Succeeded MT : MF_MT_MAJOR_TYPE = MEDIATYPE_Video; MF_MT_SUBTYPE = MFVideoFormat_IYUV; MF_MT_FRAME_SIZE = 2748779069920 (640, 480); MF_MT_FRAME_RATE = 128849018881 (30, 1); MF_MT_PIXEL_ASPECT_RATIO = 4294967297 (1, 1); MF_MT_INTERLACE_MODE = 2

  // H264 endocder only supports 1 input and 1 output stream.
  /*DWORD inMin, inMax, outMin, outMax;
  CHECK_HR(pEncoderTransfrom->GetStreamLimits(&inMin, &inMax, &outMin, &outMax), "H264 transform GetStreamLimits failed.\n");
  printf("H264 MFT stream limits %i, %i, %i, %i.\n", inMin, inMax, outMin, outMax);*/

  MFCreateMediaType(&pMFTOutputMediaType);
  pMFTOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  pMFTOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  pMFTOutputMediaType->SetUINT32(MF_MT_AVG_BITRATE, 240000);
  CHECK_HR(MFSetAttributeSize(pMFTOutputMediaType, MF_MT_FRAME_SIZE, 640, 480), "Failed to set frame size on H264 MFT out type.");
  CHECK_HR(MFSetAttributeRatio(pMFTOutputMediaType, MF_MT_FRAME_RATE, 30, 1), "Failed to set frame rate on H264 MFT out type.");
  CHECK_HR(MFSetAttributeRatio(pMFTOutputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set aspect ratio on H264 MFT out type.");
  pMFTOutputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2);
  //pMFTOutputMediaType->SetUINT32(MF_MT_MPEG2_PROFILE, 66);
  pMFTOutputMediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

  MFCreateMediaType(&pMFTInputMediaType);
  //CHECK_HR(videoSourceOutputType->CopyAllItems(pMFTInputMediaType), "Error copying media type attributes to decoder output media type.\n");
  pMFTInputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  pMFTInputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV);
  CHECK_HR(MFSetAttributeSize(pMFTInputMediaType, MF_MT_FRAME_SIZE, 640, 480), "Failed to set frame size on H264 MFT out type.");
  CHECK_HR(MFSetAttributeRatio(pMFTInputMediaType, MF_MT_FRAME_RATE, 30, 1), "Failed to set frame rate on H264 MFT out type.");
  CHECK_HR(MFSetAttributeRatio(pMFTInputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set aspect ratio on H264 MFT out type.");
  pMFTInputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2);

  std::cout << "H264 encoder output type: " << GetMediaTypeDescription(pMFTOutputMediaType) << std::endl;
  CHECK_HR(pEncoderTransfrom->SetOutputType(0, pMFTOutputMediaType, 0), "Failed to set output media type on H.264 encoder MFT.");

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

  //CHECK_HR(pDecoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL), "Failed to process FLUSH command on H.264 decoder MFT.");
  CHECK_HR(pDecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), "Failed to process BEGIN_STREAMING command on H.264 decoder MFT.");
  CHECK_HR(pDecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), "Failed to process START_OF_STREAM command on H.264 decoder MFT.");

  //CHECK_HR(pWriter->SetInputMediaType(writerVideoStreamIndex, videoSourceOutputType, NULL), "Error setting the sink writer video input type.\n");

  // Ready to go.

  printf("Reading video samples from webcam.\n");

  MFT_OUTPUT_DATA_BUFFER encDataBuffer, decDataBuffer;
  DWORD processOutputStatus = 0;
  IMFSample* videoSample = NULL;
  DWORD streamIndex, flags;
  LONGLONG llVideoTimeStamp, llSampleDuration;
  HRESULT mftEncProcessOutput = S_OK, mftDecProcessOutput = S_OK;
  MFT_OUTPUT_STREAM_INFO StreamInfo;
  IMFSample* mftEncSample = NULL, * mftDecSample = NULL;
  IMFMediaBuffer* encBuffer = NULL, * decBuffer = NULL;
  int sampleCount = 0;
  DWORD mftEncFlags, mftDecFlags;

  memset(&encDataBuffer, 0, sizeof encDataBuffer);
  memset(&decDataBuffer, 0, sizeof decDataBuffer);

  while (sampleCount <= SAMPLE_COUNT)
  {
    CHECK_HR(videoReader->ReadSample(
      MF_SOURCE_READER_FIRST_VIDEO_STREAM,
      0,                              // Flags.
      &streamIndex,                   // Receives the actual stream index. 
      &flags,                         // Receives status flags.
      &llVideoTimeStamp,              // Receives the time stamp.
      &videoSample                    // Receives the sample or NULL.
    ), "Error reading video sample.");

    if (flags & MF_SOURCE_READERF_STREAMTICK)
    {
      printf("Stream tick.\n");
    }

    if (videoSample)
    {
      printf("Sample %i.\n", sampleCount);

      CHECK_HR(videoSample->SetSampleTime(llVideoTimeStamp), "Error setting the video sample time.");
      CHECK_HR(videoSample->GetSampleDuration(&llSampleDuration), "Error getting video sample duration.");

      printf("Passing sample to the H264 encoder with sample time %I64d.\n", llVideoTimeStamp);

      // Pass the video sample to the H.264 transform.
      CHECK_HR(pEncoderTransfrom->ProcessInput(0, videoSample, 0), "The H264 encoder ProcessInput call failed.");

      CHECK_HR(pEncoderTransfrom->GetOutputStatus(&mftEncFlags), "H264 MFT GetOutputStatus failed.");

      printf("H264 get output status %.2X.\n", mftEncFlags);

      //if (mftEncFlags == MFT_OUTPUT_STATUS_SAMPLE_READY)
      //{

      mftEncProcessOutput = pEncoderTransfrom->ProcessOutput(0, 1, &encDataBuffer, &processOutputStatus);

      printf("H264 encode result %.2X, MFT status %.2X.\n", mftEncProcessOutput, processOutputStatus);

      while (mftEncProcessOutput == S_OK)
      {
        printf("H264 encoder sample ready.\n");

        CHECK_HR(MFCreateSample(&mftEncSample), "Failed to create MF sample.");

        // The buffer created with the call below is reference counted and automatically released,
        // see (https://msdn.microsoft.com/en-us/library/windows/desktop/bb530123(v=vs.85).aspx).
        CHECK_HR(pEncoderTransfrom->GetOutputStreamInfo(0, &StreamInfo), "Failed to get output stream info from H264 MFT.");
        CHECK_HR(MFCreateMemoryBuffer(StreamInfo.cbSize, &encBuffer), "Failed to create memory buffer.");

        CHECK_HR(mftEncSample->AddBuffer(encBuffer), "Failed to add sample to buffer.");
        encDataBuffer.dwStreamID = 0;
        encDataBuffer.dwStatus = 0;
        encDataBuffer.pEvents = NULL;
        encDataBuffer.pSample = mftEncSample;

        CHECK_HR(mftEncSample->SetSampleTime(llVideoTimeStamp), "Error setting MFT sample time.\n");
        CHECK_HR(mftEncSample->SetSampleDuration(llSampleDuration), "Error setting MFT sample duration.\n");

        // Pass the video sample to the H.264 decoder transform.
        CHECK_HR(pDecoderTransform->ProcessInput(0, mftEncSample, 0), "The H264 decoder ProcessInput call failed.\n");

        CHECK_HR(pDecoderTransform->GetOutputStatus(&mftDecFlags), "H264 decoder MFT GetOutputStatus failed.\n");

        // *** The MFT_OUTPUT_STATUS_SAMPLE_READY flag never seems to get set for the decoder transform...
        //if (mftDecFlags == MFT_OUTPUT_STATUS_SAMPLE_READY)
        //{
        //auto decREsult = pDecoderTransform->GetOutputStreamInfo(0, &StreamInfo), "Failed to get output stream info from H264 decoder MFT.\n");


        mftDecProcessOutput = pDecoderTransform->ProcessOutput(0, 1, &decDataBuffer, &processOutputStatus);

        while (mftDecProcessOutput == S_OK)
        {
          printf("H264 decoder sample ready.\n");

          CHECK_HR(MFCreateSample(&mftDecSample), "Failed to create MF sample.\n");
          CHECK_HR(MFCreateMemoryBuffer(StreamInfo.cbSize, &decBuffer), "Failed to create memory buffer.\n");
          CHECK_HR(mftDecSample->AddBuffer(decBuffer), "Failed to add sample to buffer.\n");
          decDataBuffer.dwStreamID = 0;
          decDataBuffer.dwStatus = 0;
          decDataBuffer.pEvents = NULL;
          decDataBuffer.pSample = mftDecSample;

          CHECK_HR(decDataBuffer.pSample->SetSampleTime(llVideoTimeStamp), "Error setting MFT sample time.\n");
          CHECK_HR(decDataBuffer.pSample->SetSampleDuration(llSampleDuration), "Error setting MFT sample duration.\n");

          IMFMediaBuffer* buf = NULL;
          DWORD bufLength;
          CHECK_HR(decDataBuffer.pSample->ConvertToContiguousBuffer(&buf), "ConvertToContiguousBuffer failed.\n");
          CHECK_HR(buf->GetCurrentLength(&bufLength), "Get buffer length failed.\n");

          printf("Buffer size %i.\n", bufLength);

          byte* byteBuffer;
          DWORD buffCurrLen = 0;
          DWORD buffMaxLen = 0;
          buf->Lock(&byteBuffer, &buffMaxLen, &buffCurrLen);
          outputBuffer.write((char*)byteBuffer, bufLength);
          outputBuffer.flush();
        }

        mftEncProcessOutput = pEncoderTransfrom->ProcessOutput(0, 1, &encDataBuffer, &processOutputStatus);
      }
    }

    sampleCount++;
  }

done:

  outputBuffer.close();

  printf("finished.\n");
  auto c = getchar();

  return 0;
}
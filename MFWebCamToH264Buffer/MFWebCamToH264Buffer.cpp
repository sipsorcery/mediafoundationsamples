/******************************************************************************
* Filename: MFWebCamToH264Buffer.cpp
*
* Description:
* This file contains a C++ console application that captures the realtime video
* stream from a webcam to an H264 byte array. Rather than using a black box
* sink writer to do the H264 encoding a media foundation transform is employed.
* This allows more flexibility about what can be done with the encoded samples.
* For example they could be packetised in RTP packets and transmitted over a
* network.
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 26 Feb 2015	  Aaron Clauson	  Created, Hobart, Australia.
*
* License: Public Domain (no warranty, use at own risk)
/******************************************************************************/

#include "..\Common\MFUtility.h"

#include <stdio.h>
#include <tchar.h>
#include <evr.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wmcodecdsp.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

#define WEBCAM_DEVICE_INDEX 0			// Set to 0 to use default system webcam.
#define CAPTURE_FILENAME L"sample.mp4"
#define SAMPLE_COUNT 100
#define OUTPUT_FRAME_WIDTH 640		// Adjust if the webcam does not support this frame width.
#define OUTPUT_FRAME_HEIGHT 480		// Adjust if the webcam does not support this frame height.

int _tmain(int argc, _TCHAR* argv[])
{
  IMFMediaSource* pVideoSource = NULL;
  IMFSourceReader* pVideoReader = NULL;
  WCHAR* webcamFriendlyName;
  IMFMediaType* videoSourceOutputType = NULL, * pSrcOutMediaType = NULL;
  IUnknown* spTransformUnk = NULL;
  IMFTransform* pTransform = NULL; //< this is H264 Encoder MFT
  IMFMediaType* pMFTInputMediaType = NULL, * pMFTOutputMediaType = NULL;
  IMFSinkWriter* pWriter = NULL;
  IMFMediaType* pVideoOutType = NULL;
  DWORD writerVideoStreamIndex = 0;
  DWORD totalSampleBufferSize = 0;
  DWORD mftStatus = 0;
  UINT webcamNameLength = 0;

  CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
    "COM initialisation failed.");

  CHECK_HR(MFStartup(MF_VERSION),
    "Media Foundation initialisation failed.");

  // Get video capture devices.
  CHECK_HR(GetVideoSourceFromDevice(WEBCAM_DEVICE_INDEX, &pVideoSource, &pVideoReader),
    "Failed to get webcam video source.");

  CHECK_HR(pVideoReader->GetCurrentMediaType(
    (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
    &videoSourceOutputType), "Error retrieving current media type from first video stream.");

  // Note the webcam needs to support this media type. The list of media types supported can be obtained using the ListTypes function in MFUtility.h.
  CHECK_HR(MFCreateMediaType(&pSrcOutMediaType), "Failed to create media type.");
  CHECK_HR(pSrcOutMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set major video type.");
  CHECK_HR(pSrcOutMediaType->SetGUID(MF_MT_SUBTYPE, WMMEDIASUBTYPE_I420), "Failed to set video sub type to I420.");
  CHECK_HR(MFSetAttributeRatio(pSrcOutMediaType, MF_MT_FRAME_RATE, 30, 1), "Failed to set frame rate on source reader out type.");
  CHECK_HR(MFSetAttributeSize(pSrcOutMediaType, MF_MT_FRAME_SIZE, OUTPUT_FRAME_WIDTH, OUTPUT_FRAME_HEIGHT), "Failed to set frame size.");

  CHECK_HR(pVideoReader->SetCurrentMediaType(0, NULL, pSrcOutMediaType), "Failed to set media type on source reader.");

  // Create H.264 encoder.
  CHECK_HR(CoCreateInstance(CLSID_CMSH264EncoderMFT, NULL, CLSCTX_INPROC_SERVER,
    IID_IUnknown, (void**)&spTransformUnk), "Failed to create H264 encoder MFT.");

  CHECK_HR(spTransformUnk->QueryInterface(IID_PPV_ARGS(&pTransform)),
    "Failed to get IMFTransform interface from H264 encoder MFT object.");

  CHECK_HR(MFCreateMediaType(&pMFTOutputMediaType), "Failed to create media type.");
  CHECK_HR(pMFTOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set major video type.");
  CHECK_HR(pMFTOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264), "Failed to set video sub type to H264.");
  CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_MT_AVG_BITRATE, 240000), "Failed to set video avg bit rate.");
  CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2), "Failed to set video interlace mode.");
  CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE), "Failed to set all samples independent.");
  CHECK_HR(CopyAttribute(pSrcOutMediaType, pMFTOutputMediaType, MF_MT_FRAME_RATE), "Failed to copy frame rate media attribute.");
  CHECK_HR(CopyAttribute(pSrcOutMediaType, pMFTOutputMediaType, MF_MT_FRAME_SIZE), "Failed to copy frame size media attribute.");

  CHECK_HR(pTransform->SetOutputType(0, pMFTOutputMediaType, 0),
    "Failed to set output media type on H.264 encoder MFT.");

  CHECK_HR(MFCreateMediaType(&pMFTInputMediaType), "Failed to create media type.");
  CHECK_HR(pSrcOutMediaType->CopyAllItems(pMFTInputMediaType),
    "Failed to copy media type from source output to mft input type.");
  CHECK_HR(pMFTInputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV), "Failed to set video sub type to YUV.");

  CHECK_HR(pTransform->SetInputType(0, pMFTInputMediaType, 0),
    "Failed to set input media type on H.264 encoder MFT.");

  CHECK_HR(pTransform->GetInputStatus(0, &mftStatus),
    "Failed to get input status from H.264 MFT.");

  if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
    printf("E: ApplyTransform() pTransform->GetInputStatus() not accept data.\n");
    goto done;
  }

  CHECK_HR(pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), "Failed to process BEGIN_STREAMING command on H.264 MFT.");
  CHECK_HR(pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), "Failed to process START_OF_STREAM command on H.264 MFT.");

  // Create the MP4 sink writer.
  CHECK_HR(MFCreateSinkWriterFromURL(
    CAPTURE_FILENAME,
    NULL,
    NULL,
    &pWriter),
    "Error creating mp4 sink writer.");

  // Configure the output video type on the sink writer.
  CHECK_HR(MFCreateMediaType(&pVideoOutType), "Configure encoder failed to create media type for video output sink.");

  CHECK_HR(pMFTOutputMediaType->CopyAllItems(pVideoOutType),
    "Failed to copy all media types from MFT output to sink input.");

  CHECK_HR(pWriter->AddStream(pVideoOutType, &writerVideoStreamIndex),
    "Failed to add the video stream to the sink writer.");

  pVideoOutType->Release();

  // Ready to go.

  CHECK_HR(pWriter->BeginWriting(),
    "Sink writer begin writing call failed.");

  printf("Reading video samples from webcam.\n");

  MFT_OUTPUT_DATA_BUFFER outputDataBuffer;
  DWORD processOutputStatus = 0;
  IMFSample* videoSample = NULL;
  DWORD streamIndex, flags;
  LONGLONG llVideoTimeStamp, llSampleDuration;
  MFT_OUTPUT_STREAM_INFO StreamInfo;
  IMFSample* mftOutSample = NULL;
  IMFMediaBuffer* pBuffer = NULL;
  int sampleCount = 0;
  DWORD mftOutFlags;

  while (sampleCount <= SAMPLE_COUNT)
  {
    CHECK_HR(pVideoReader->ReadSample(
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
      pWriter->SendStreamTick(0, llVideoTimeStamp);
    }

    if (videoSample)
    {
      CHECK_HR(videoSample->SetSampleTime(llVideoTimeStamp), "Error setting the video sample time.\n");
      CHECK_HR(videoSample->GetSampleDuration(&llSampleDuration), "Error getting video sample duration.\n");

      // Pass the video sample to the H.264 transform.

      CHECK_HR(pTransform->ProcessInput(0, videoSample, 0), "The resampler H264 ProcessInput call failed.\n");

      auto mftResult = S_OK;

      while (mftResult == S_OK)
      {
        CHECK_HR(MFCreateSample(&mftOutSample), "Failed to create MF sample.");
        CHECK_HR(pTransform->GetOutputStreamInfo(0, &StreamInfo), "Failed to get output stream info from H264 MFT.");
        CHECK_HR(MFCreateMemoryBuffer(StreamInfo.cbSize, &pBuffer), "Failed to create memory buffer.");
        CHECK_HR(mftOutSample->AddBuffer(pBuffer), "Failed to add sample to buffer.");
        outputDataBuffer.dwStreamID = 0;
        outputDataBuffer.dwStatus = 0;
        outputDataBuffer.pEvents = NULL;
        outputDataBuffer.pSample = mftOutSample;

        mftResult = pTransform->ProcessOutput(0, 1, &outputDataBuffer, &processOutputStatus);

        if (mftResult == S_OK)
        {
          CHECK_HR(outputDataBuffer.pSample->SetSampleTime(llVideoTimeStamp), "Error setting MFT sample time.\n");
          CHECK_HR(outputDataBuffer.pSample->SetSampleDuration(llSampleDuration), "Error setting MFT sample duration.\n");

          IMFMediaBuffer* buf = NULL;
          DWORD bufLength;
          CHECK_HR(mftOutSample->ConvertToContiguousBuffer(&buf), "ConvertToContiguousBuffer failed.\n");
          CHECK_HR(buf->GetCurrentLength(&bufLength), "Get buffer length failed.\n");

          totalSampleBufferSize += bufLength;

          printf("Writing sample %i, sample time %I64d, sample duration %I64d, sample size %i.\n", sampleCount, llVideoTimeStamp, llSampleDuration, bufLength);
          CHECK_HR(pWriter->WriteSample(writerVideoStreamIndex, outputDataBuffer.pSample), "The stream sink writer was not happy with the sample.\n");

          SAFE_RELEASE(buf);
        }

        SAFE_RELEASE(pBuffer);
        SAFE_RELEASE(mftOutSample);
      }

      SAFE_RELEASE(&videoSample);

      if (mftResult != MF_E_TRANSFORM_NEED_MORE_INPUT) {
        // Error condition.
        printf("H264 encoder error on process output %.2X.", mftResult);
        break;
      }
    }

    sampleCount++;
  }

  printf("Total sample buffer size %i.\n", totalSampleBufferSize);
  printf("Finalising the capture.\n");

  if (pWriter)
  {
    // See http://stackoverflow.com/questions/24411737/media-foundation-imfsinkwriterfinalize-method-fails-under-windows-7-when-mux 
    // for why the Finalize call can fail with MF_E_ATTRIBUTENOTFOUND.
    CHECK_HR(pWriter->Finalize(), "Error finalising H.264 sink writer.\n");
  }

done:

  printf("finished.\n");
  auto c = getchar();

  SAFE_RELEASE(pVideoSource);
  SAFE_RELEASE(pVideoReader);
  SAFE_RELEASE(videoSourceOutputType);
  SAFE_RELEASE(pSrcOutMediaType);
  SAFE_RELEASE(spTransformUnk);
  SAFE_RELEASE(pTransform);
  SAFE_RELEASE(pMFTInputMediaType);
  SAFE_RELEASE(pMFTOutputMediaType);
  SAFE_RELEASE(pWriter);

  return 0;
}
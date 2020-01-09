/******************************************************************************
* Filename: MFMP4ToYUVWithMFT.cpp
*
* Description:
* This file contains a C++ console application that reads H264 encoded video
* frames from an mp4 file and decodes them to a YUV pixel format and dumps them
* to an output file stream.
*
* Note: If using the big_buck_bunny.mp4 file as the source the frame size changes
* from 640x360 to 640x368 on the 25th sample. This program deals with it by throwing
* overwriting the captured file whenever the source stream changes.
*
* To convert the raw yuv data dumped at the end of this sample use the ffmpeg command below:
* ffmpeg -vcodec rawvideo -s 640x368 -pix_fmt yuv420p -i rawframes.yuv -vframes 1 output.jpeg
* ffmpeg -vcodec rawvideo -s 640x368 -pix_fmt yuv420p -i rawframes.yuv out.avi
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 08 Mar 2015	  Aaron Clauson	  Created, Hobart, Australia.
* 08 Jan 2020   Aaron Clauson   Added ability to cope with source stream change event.
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

#include <fstream>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

// Forward function definitions.
HRESULT CreateSingleBufferIMFSample(DWORD bufferSize, IMFSample** pSample);
HRESULT CreateAndCopySingleBufferIMFSample(IMFSample* pSrcSample, IMFSample** pDstSample);
HRESULT TransformSample(IMFTransform* pTransform, IMFSample* pSample, IMFSample** pOutSample, BOOL* transformFlushed);
HRESULT WriteSampleToFile(IMFSample* pSample, std::ofstream* pFileStream);

#define VIDEO_SAMPLE_WIDTH 640	// Needs to match the video frame width in the input file.
#define VIDEO_SAMPLE_HEIGHT 360 // Needs to match the video frame height in the input file.
#define SAMPLE_COUNT 100
#define SOURCE_FILENAME L"../MediaFiles/big_buck_bunny.mp4"
#define CAPTURE_FILENAME "rawframes.yuv"

int _tmain(int argc, _TCHAR* argv[])
{
  std::ofstream outputBuffer(CAPTURE_FILENAME, std::ios::out | std::ios::binary);

  CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
    "COM initialisation failed.");

  CHECK_HR(MFStartup(MF_VERSION),
    "Media Foundation initialisation failed.");

  IMFSourceResolver* pSourceResolver = NULL;
  IUnknown* uSource = NULL;
  IMFMediaSource* mediaFileSource = NULL;
  IMFAttributes* pVideoReaderAttributes = NULL;
  IMFSourceReader* pSourceReader = NULL;
  IMFMediaType* pFileVideoMediaType = NULL;
  IUnknown* spDecTransformUnk = NULL;
  IMFTransform* pDecoderTransform = NULL; // This is H264 Decoder MFT.
  IMFMediaType* pDecInputMediaType = NULL, * pDecOutputMediaType = NULL;
  MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;
  DWORD mftStatus = 0;

  // Set up the reader for the file.
  CHECK_HR(MFCreateSourceResolver(&pSourceResolver),
    "MFCreateSourceResolver failed.");

  CHECK_HR(pSourceResolver->CreateObjectFromURL(
    SOURCE_FILENAME,		        // URL of the source.
    MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
    NULL,                       // Optional property store.
    &ObjectType,				        // Receives the created object type. 
    &uSource					          // Receives a pointer to the media source.
  ),
    "Failed to create media source resolver for file.");

  CHECK_HR(uSource->QueryInterface(IID_PPV_ARGS(&mediaFileSource)),
    "Failed to create media file source.");

  CHECK_HR(MFCreateAttributes(&pVideoReaderAttributes, 2),
    "Failed to create attributes object for video reader.");

  CHECK_HR(pVideoReaderAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID),
    "Failed to set dev source attribute type for reader config.");

  CHECK_HR(pVideoReaderAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, 1),
    "Failed to set enable video processing attribute type for reader config.");

  CHECK_HR(MFCreateSourceReaderFromMediaSource(mediaFileSource, pVideoReaderAttributes, &pSourceReader),
    "Error creating media source reader.");

  CHECK_HR(pSourceReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pFileVideoMediaType),
    "Error retrieving current media type from first video stream.");

  // Create H.264 decoder.
  CHECK_HR(CoCreateInstance(CLSID_CMSH264DecoderMFT, NULL, CLSCTX_INPROC_SERVER,
    IID_IUnknown, (void**)&spDecTransformUnk),
    "Failed to create H264 decoder MFT.");

  CHECK_HR(spDecTransformUnk->QueryInterface(IID_PPV_ARGS(&pDecoderTransform)), "Failed to get IMFTransform interface from H264 decoder MFT object.");

  MFCreateMediaType(&pDecInputMediaType);
  CHECK_HR(pFileVideoMediaType->CopyAllItems(pDecInputMediaType), "Error copying media type attributes to decoder input media type.");
  CHECK_HR(pDecoderTransform->SetInputType(0, pDecInputMediaType, 0), "Failed to set input media type on H.264 decoder MFT.");

  MFCreateMediaType(&pDecOutputMediaType);
  CHECK_HR(pFileVideoMediaType->CopyAllItems(pDecOutputMediaType), "Error copying media type attributes to decoder input media type.");
  CHECK_HR(pDecOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV), "Failed to set media sub type.");

  CHECK_HR(pDecoderTransform->SetOutputType(0, pDecOutputMediaType, 0), "Failed to set output media type on H.264 decoder MFT.");

  CHECK_HR(pDecoderTransform->GetInputStatus(0, &mftStatus), "Failed to get input status from H.264 decoder MFT.");
  if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
    printf("H.264 decoder MFT is not accepting data.\n");
    goto done;
  }

  std::cout << "H264 decoder output media type: " << GetMediaTypeDescription(pDecOutputMediaType) << std::endl << std::endl;

  CHECK_HR(pDecoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL), "Failed to process FLUSH command on H.264 decoder MFT.");
  CHECK_HR(pDecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), "Failed to process BEGIN_STREAMING command on H.264 decoder MFT.");
  CHECK_HR(pDecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), "Failed to process START_OF_STREAM command on H.264 decoder MFT.");

  // Start processing frames.
  IMFSample* pVideoSample = NULL, * pCopyVideoSample = NULL, * pH264DecodeOutSample = NULL;
  DWORD streamIndex, flags;
  LONGLONG llVideoTimeStamp = 0, llSampleDuration = 0, yuvVideoTimeStamp = 0, yuvSampleDuration = 0;
  int sampleCount = 0;
  DWORD sampleFlags = 0;
  BOOL h264DecodeTransformFlushed = FALSE;

  while (sampleCount <= SAMPLE_COUNT)
  {
    CHECK_HR(pSourceReader->ReadSample(
      MF_SOURCE_READER_FIRST_VIDEO_STREAM,
      0,                              // Flags.
      &streamIndex,                   // Receives the actual stream index. 
      &flags,                         // Receives status flags.
      &llVideoTimeStamp,              // Receives the time stamp.
      &pVideoSample                    // Receives the sample or NULL.
    ), "Error reading video sample.");

    if (flags & MF_SOURCE_READERF_STREAMTICK)
    {
      printf("Stream tick.\n");
    }

    if (pVideoSample)
    {
      printf("Processing sample %i.\n", sampleCount);

      CHECK_HR(pVideoSample->SetSampleTime(llVideoTimeStamp), "Error setting the video sample time.");
      CHECK_HR(pVideoSample->GetSampleDuration(&llSampleDuration), "Error getting video sample duration.");
      CHECK_HR(pVideoSample->GetSampleFlags(&sampleFlags), "Error getting smaple flags.");

      printf("Sample count %d, Sample flags %d, sample duration %I64d, sample time %I64d\n", sampleCount, sampleFlags, llSampleDuration, llVideoTimeStamp);

      // Replicate transmitting the sample across the network and reconstructing.
      CHECK_HR(CreateAndCopySingleBufferIMFSample(pVideoSample, &pCopyVideoSample),
        "Failed to copy single buffer IMF sample.");

      // Apply the H264 decoder transform
      CHECK_HR(TransformSample(pDecoderTransform, pCopyVideoSample, &pH264DecodeOutSample, &h264DecodeTransformFlushed),
        "Failed to apply H24 decoder transform.");

      if (h264DecodeTransformFlushed == TRUE) {
        // H264 decoder format changed. Clear the capture file and start again.
        outputBuffer.close();
        outputBuffer.open(CAPTURE_FILENAME, std::ios::out | std::ios::binary);
      }
      else if (pH264DecodeOutSample != NULL) {
        // Write decoded sample to capture file.
        CHECK_HR(WriteSampleToFile(pH264DecodeOutSample, &outputBuffer),
          "Failed to write sample to file.");
      }

      sampleCount++;

      SAFE_RELEASE(pH264DecodeOutSample);
      SAFE_RELEASE(pCopyVideoSample);
    }
  }

  outputBuffer.close();

done:

  printf("finished.\n");
  auto c = getchar();

  SAFE_RELEASE(pSourceResolver);
  SAFE_RELEASE(uSource);
  SAFE_RELEASE(mediaFileSource);
  SAFE_RELEASE(pVideoReaderAttributes);
  SAFE_RELEASE(pSourceReader);
  SAFE_RELEASE(pFileVideoMediaType);
  SAFE_RELEASE(spDecTransformUnk);
  SAFE_RELEASE(pDecoderTransform);
  SAFE_RELEASE(pDecInputMediaType),
  SAFE_RELEASE(pDecOutputMediaType);

  return 0;
}

/**
* Dumps the media buffer contents of an IMF sample to a file stream.
* @param[in] pSample: pointer to the media sample to dump the contents from.
* @param[in] pFileStream: pointer to the file stream to wrtie to.
* @@Returns S_OK if successful or an error code if not.
*/
HRESULT WriteSampleToFile(IMFSample* pSample, std::ofstream* pFileStream)
{
  IMFMediaBuffer* buf = NULL;
  DWORD bufLength;

  HRESULT hr = S_OK;

  hr = pSample->ConvertToContiguousBuffer(&buf);
  CHECK_HR(hr, "ConvertToContiguousBuffer failed.");

  hr = buf->GetCurrentLength(&bufLength);
  CHECK_HR(hr, "Get buffer length failed.");

  printf("Writing sample to capture file sample size %i.\n", bufLength);

  byte* byteBuffer = NULL;
  DWORD buffMaxLen = 0, buffCurrLen = 0;
  buf->Lock(&byteBuffer, &buffMaxLen, &buffCurrLen);

  pFileStream->write((char*)byteBuffer, bufLength);
  pFileStream->flush();

done:

  SAFE_RELEASE(buf);

  return hr;
}

/**
* Applies an MFT taransform to a media sample.
* @param[in] pTransform: pointer to the media transform to apply.
* @param[in] pSample: pointer to the media sample to apply the transform to.
* @param[out] pOutSample: pointer to the media sample output by the transform. Can be NULL
*                        if the transform did not produce one.
* @param[out] transformFlushed: if set to true means the transform format changed and the
                                contents were flushed. Output format of sample most likely changed.
* @@Returns S_OK if successful or an error code if not.
*/
HRESULT TransformSample(IMFTransform* pTransform, IMFSample* pSample, IMFSample** pOutSample, BOOL* transformFlushed)
{
  MFT_OUTPUT_STREAM_INFO StreamInfo;
  MFT_OUTPUT_DATA_BUFFER outputDataBuffer = {};
  DWORD processOutputStatus = 0;
  IMFMediaType* pChangedOutMediaType = NULL;

  HRESULT hr = S_OK;
  *transformFlushed = FALSE;

  hr = pTransform->ProcessInput(0, pSample, 0);
  CHECK_HR(hr, "The H264 decoder ProcessInput call failed.");

  hr = pTransform->GetOutputStreamInfo(0, &StreamInfo);
  CHECK_HR(hr, "Failed to get output stream info from H264 MFT.");

  hr = CreateSingleBufferIMFSample(StreamInfo.cbSize, pOutSample);
  CHECK_HR(hr, "Failed to create new single buffer IMF sample.");

  outputDataBuffer.dwStreamID = 0;
  outputDataBuffer.dwStatus = 0;
  outputDataBuffer.pEvents = NULL;
  outputDataBuffer.pSample = *pOutSample;

  auto mftProcessOutput = pTransform->ProcessOutput(0, 1, &outputDataBuffer, &processOutputStatus);

  printf("Process output result %.2X, MFT status %.2X.\n", mftProcessOutput, processOutputStatus);

  if (mftProcessOutput == MF_E_TRANSFORM_STREAM_CHANGE) {
    // Format of the input stream has changed. https://docs.microsoft.com/en-us/windows/win32/medfound/handling-stream-changes
    if (outputDataBuffer.dwStatus == MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE) {
      printf("H264 stream changed.\n");

      hr = pTransform->GetOutputAvailableType(0, 0, &pChangedOutMediaType);
      CHECK_HR(hr, "Failed to get the H264 decoder ouput media type after a stream change.");

      std::cout << "H264 decoder output media type: " << GetMediaTypeDescription(pChangedOutMediaType) << std::endl << std::endl;

      hr = pChangedOutMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV);
      CHECK_HR(hr, "Failed to set media sub type.");

      hr = pTransform->SetOutputType(0, pChangedOutMediaType, 0);
      CHECK_HR(hr, "Failed to set new output media type on H.264 decoder MFT.");

      hr = pTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL);
      CHECK_HR(hr, "Failed to process FLUSH command on H.264 decoder MFT.");

      *transformFlushed = TRUE;
    }
    else {
      printf("H264 stream changed but didn't have the data format change flag set. Don't know what to do.\n");
      hr = E_NOTIMPL;
    }
  }
  else if (mftProcessOutput != S_OK && mftProcessOutput != MF_E_TRANSFORM_NEED_MORE_INPUT) {
    printf("H264 decoder process output error result %.2X, MFT status %.2X.\n", mftProcessOutput, processOutputStatus);
    hr = mftProcessOutput;
  }

done:

  SAFE_RELEASE(pChangedOutMediaType);

  return hr;
}

/**
* Creates a new single buffer media sample.
* @param[in] bufferSize: size of the media buffer to set on the create media sample.
* @param[out] pSample: pointer to the create single buffer media sample.
* @@Returns S_OK if successful or an error code if not.
*/
HRESULT CreateSingleBufferIMFSample(DWORD bufferSize, IMFSample** pSample)
{
  IMFMediaBuffer* pBuffer = NULL;

  HRESULT hr = S_OK;

  hr = MFCreateSample(pSample);
  CHECK_HR(hr, "Failed to create MF sample.");

  hr = MFCreateMemoryBuffer(bufferSize, &pBuffer);
  CHECK_HR(hr, "Failed to create memory buffer.");

  hr = (*pSample)->AddBuffer(pBuffer);
  CHECK_HR(hr, "Failed to add sample to buffer.");

done:

  return hr;
}

/**
* Creates a new media smaple and vopies the first media buffer from the source to it.
* @param[in] pSrcSampl: size of the media buffer to set on the create media sample.
* @param[out] pDstSample: pointer to the the media sample created.
* @@Returns S_OK if successful or an error code if not.
*/
HRESULT CreateAndCopySingleBufferIMFSample(IMFSample* pSrcSample, IMFSample** pDstSample)
{
  IMFMediaBuffer* pSrcBuf = NULL;
  IMFMediaBuffer* pDstBuffer = NULL;
  DWORD srcBufLength;

  HRESULT hr = S_OK;

  // Gets total length of ALL media buffer samples. We can use here because it's only a
  // single buffer sample copy.
  hr = pSrcSample->GetTotalLength(&srcBufLength);
  CHECK_HR(hr, "Failed to get total length from source buffer.");

  hr = CreateSingleBufferIMFSample(srcBufLength, pDstSample);
  CHECK_HR(hr, "Failed to create new single buffer IMF sample.");

  hr = pSrcSample->CopyAllItems(*pDstSample);
  CHECK_HR(hr, "Failed to copy IMFSample items from src to dst.");

  hr = (*pDstSample)->GetBufferByIndex(0, &pDstBuffer);
  CHECK_HR(hr, "Failed to get buffer from sample.");

  hr = pSrcSample->CopyToBuffer(pDstBuffer);
  CHECK_HR(hr, "Failed to copy IMF media buffer.");

done:

  return hr;
}
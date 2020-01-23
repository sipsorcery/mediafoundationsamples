/******************************************************************************
* Filename: MFMP4ToYUVWithoutMFT.cpp
*
* Description:
* The sample is based on the MFMP4ToYUVWithMFT one. The sample gets YUV
* samples out of the Source Reader WITHOUT having to explicitly wire up
* the H264 decoder transform.
* 
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
* 23 Jan 2020	  Aaron Clauson	  Created, Dublin, Ireland. Based on MFMP4ToYUVWithMFT sample.
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
  IMFMediaType* pReaderOutputType = NULL, *pFirstOutputType = NULL;
  MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;
  DWORD mftStatus = 0;

  // Need the color converter DSP for conversions between YUV, RGB etc.
  // Lots of examples use this explicit colour converter load. Seems
  // like most cases it gets loaded automatically.
 /* CHECK_HR(MFTRegisterLocalByCLSID(
    __uuidof(CColorConvertDMO),
    MFT_CATEGORY_VIDEO_PROCESSOR,
    L"",
    MFT_ENUM_FLAG_SYNCMFT,
    0,
    NULL,
    0,
    NULL),
    "Error registering colour converter DSP.");*/

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

  CHECK_HR(pVideoReaderAttributes->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV),
    "Failed to set media sub type on source reader output media type.");

  CHECK_HR(MFCreateSourceReaderFromMediaSource(mediaFileSource, pVideoReaderAttributes, &pSourceReader),
    "Error creating media source reader.");

  CHECK_HR(MFCreateMediaType(&pReaderOutputType), "Failed to create source reader output media type.");
  CHECK_HR(pReaderOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set major type on source reader output media type.");
  CHECK_HR(pReaderOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV), "Failed to set media sub type on source reader output media type.");

  CHECK_HR(pSourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pReaderOutputType),
    "Failed to set output media type on source reader.");

  CHECK_HR(pSourceReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pFirstOutputType),
    "Error retrieving current media type from first video stream.");

  std::cout << "Source reader output media type: " << GetMediaTypeDescription(pFirstOutputType) << std::endl << std::endl;

  // Start processing frames.
  IMFSample* pVideoSample = NULL;
  DWORD streamIndex, flags;
  LONGLONG llVideoTimeStamp = 0, llSampleDuration = 0;
  int sampleCount = 0;
  DWORD sampleFlags = 0;

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
      //break;
    }

    if (pVideoSample)
    {
      printf("Processing sample %i.\n", sampleCount);

      CHECK_HR(pVideoSample->SetSampleTime(llVideoTimeStamp), "Error setting the video sample time.");
      CHECK_HR(pVideoSample->GetSampleDuration(&llSampleDuration), "Error getting video sample duration.");
      CHECK_HR(pVideoSample->GetSampleFlags(&sampleFlags), "Error getting sample flags.");

      printf("Sample count %d, Sample flags %d, sample duration %I64d, sample time %I64d\n", sampleCount, sampleFlags, llSampleDuration, llVideoTimeStamp);

      CHECK_HR(WriteSampleToFile(pVideoSample, &outputBuffer),
        "Failed to write sample to file.");

      sampleCount++;
    }

    SAFE_RELEASE(pVideoSample);
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
  SAFE_RELEASE(pReaderOutputType);
  SAFE_RELEASE(pFirstOutputType);

  return 0;
}
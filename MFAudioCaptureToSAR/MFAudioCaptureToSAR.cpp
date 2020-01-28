/******************************************************************************
* Filename: MFAudioCaptureToSAR.cpp
*
* Description:
* This file contains a C++ console application that is attempting to play the
* audio from a capture device using the Windows Media Foundation API. 
* Playback is done using the Streaming Audio Renderer:
* https://msdn.microsoft.com/en-us/library/windows/desktop/aa369729%28v=vs.85%29.aspx
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 27 Jan 2020	  Aaron Clauson	  Created, Dublin, Ireland.
*
* License: Public Domain (no warranty, use at own risk)
/******************************************************************************/

#include "..\Common\MFUtility.h"

#include <d3d9.h>
#include <Dxva2api.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfobjects.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mmdeviceapi.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <windowsx.h>

#include <iostream>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "evr.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#define AUDIO_CAPTURE_DEVICE_INDEX 0	    // Adjust according to desired audio capture device.
#define AUDIO_OUTPUT_DEVICE_INDEX 0       // Adjust according to desired audio output device.

int main()
{
  IMFMediaSource* pAudioSource = NULL;
  IMFSourceReader* pAudioReader = NULL;
  IMFMediaSink* pAudioSink = NULL;
  IMFStreamSink* pStreamSink = NULL;
  IMFSinkWriter* pAudioSinkWriter = NULL;
  IMFMediaTypeHandler* pSinkMediaTypeHandler = NULL;
  IMFMediaType* pSinkSupportedType = NULL;
  BOOL fSelected = false;
  DWORD sourceStreamCount = 0, sinkStreamCount = 0, sinkStreamIndex = 0, sinkMediaTypeCount = 0;

  CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
    "COM initialisation failed.");

  CHECK_HR(MFStartup(MF_VERSION),
    "Media Foundation initialisation failed.");

  /*CHECK_HR(ListAudioOutputDevices(),
    "Failed to list audio output devices.");*/

  //CHECK_HR(ListCaptureDevices(DeviceType::Audio),
  //  "Error listing audio capture devices.");

  // ----- Set up audio capture (microphone) source. -----

  CHECK_HR(GetSourceFromCaptureDevice(DeviceType::Audio, AUDIO_CAPTURE_DEVICE_INDEX, &pAudioSource, &pAudioReader),
    "Failed to get microphone source reader.");

  CHECK_HR(pAudioReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE),
    "Failed to set the first audio stream on the source reader.");

  // ----- Set up Audio sink (Streaming Audio Renderer). -----

  CHECK_HR(GetAudioOutputDevice(AUDIO_OUTPUT_DEVICE_INDEX, &pAudioSink),
    "Failed to create streaming audio sink.");

  CHECK_HR(pAudioSink->GetStreamSinkByIndex(0, &pStreamSink),
    "Failed to get audio sink stream by index.");

  CHECK_HR(pStreamSink->GetMediaTypeHandler(&pSinkMediaTypeHandler),
    "Failed to get media type handler for stream sink.");

  CHECK_HR(pSinkMediaTypeHandler->GetMediaTypeCount(&sinkMediaTypeCount),
    "Error getting sink media type count.");

  std::cout << "Sink media type count " << sinkMediaTypeCount << "." << std::endl;

  // ----- Wire up the source and sink. -----

  // Find a media type that the stream sink supports.
  for (UINT i = 0; i < sinkMediaTypeCount; i++)
  {
    CHECK_HR(pSinkMediaTypeHandler->GetMediaTypeByIndex(i, &pSinkSupportedType),
      "Error getting media type from sink media type handler.");

    if (pSinkMediaTypeHandler->IsMediaTypeSupported(pSinkSupportedType, NULL) == S_OK)
    {
      std::cout << "Matching media type found." << std::endl;
      std::cout << GetMediaTypeDescription(pSinkSupportedType) << std::endl;
      break;
    }
    else {
      std::cout << "Sink and source media type incompatible." << std::endl;
      //std::cout << GetMediaTypeDescription(pSinkSupportedType) << std::endl;
      SAFE_RELEASE(pSinkSupportedType);
    }
  }

  if (pSinkSupportedType != NULL) {

      // Set the supported type on the reader.
    CHECK_HR(pAudioReader->SetCurrentMediaType(0, NULL, pSinkSupportedType),
      "Failed to set media type on reader.");

    CHECK_HR(MFCreateSinkWriterFromMediaSink(pAudioSink, NULL, &pAudioSinkWriter),
      "Failed to create sink writer from audio sink.");

    CHECK_HR(pAudioSinkWriter->SetInputMediaType(0, pSinkSupportedType, NULL),
      "Error setting sink media type.");

    CHECK_HR(pAudioSinkWriter->BeginWriting(),
      "Failed to being writing on audio sink writer.");

    // ----- Source and sink now configured. Set up remaining infrastructure and then start sampling. -----

    // Start the sample read-write loop.
    IMFSample* pAudioSample = NULL;
    DWORD streamIndex, flags;
    LONGLONG llTimeStamp;

    while (true)
    {
      CHECK_HR(pAudioReader->ReadSample(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        0,                              // Flags.
        &streamIndex,                   // Receives the actual stream index. 
        &flags,                         // Receives status flags.
        &llTimeStamp,                   // Receives the time stamp.
        &pAudioSample                   // Receives the sample or NULL.
      ), "Error reading audio sample.");

      if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
      {
        printf("End of stream.\n");
        break;
      }
      if (flags & MF_SOURCE_READERF_STREAMTICK)
      {
        printf("Stream tick.\n");
        CHECK_HR(pAudioSinkWriter->SendStreamTick(0, llTimeStamp), "Error sending stream tick.");
      }

      if (!pAudioSample)
      {
        printf("Null audio sample.\n");
      }
      else
      {
        LONGLONG sampleDuration = 0;

        CHECK_HR(pAudioSample->SetSampleTime(llTimeStamp), "Error setting the audio sample time.");
        CHECK_HR(pAudioSample->GetSampleDuration(&sampleDuration), "Failed to get audio sample duration.");

        printf("Audio sample, duration %llu, sample time %llu.\n", sampleDuration, llTimeStamp);

        CHECK_HR(pAudioSinkWriter->WriteSample(0, pAudioSample), "Sink writer write sample failed.");
      }

      SAFE_RELEASE(pAudioSample);
    }
  }

done:

  printf("finished.\n");
  auto c = getchar();

  SAFE_RELEASE(pAudioSource);
  SAFE_RELEASE(pAudioReader);
  SAFE_RELEASE(pAudioSink);
  SAFE_RELEASE(pSinkSupportedType);
  SAFE_RELEASE(pStreamSink);
  SAFE_RELEASE(pAudioSinkWriter);
  SAFE_RELEASE(pSinkMediaTypeHandler);

  return 0;
}

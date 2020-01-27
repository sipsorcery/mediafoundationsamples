/******************************************************************************
* Filename: MFAudioCaptureToSAR.cpp
*
* Description:
* This file contains a C++ console application that is attempting to play the
* audio from a capture device using the Windows Media Foundation
* API. Playback is done using the Streaming Audio Renderer:
* https://msdn.microsoft.com/en-us/library/windows/desktop/aa369729%28v=vs.85%29.aspx
*
* Status:
* Not working. No audio played on output device.
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
#pragma comment(lib, "Strmiids")

#define AUDIO_CAPTURE_DEVICE_INDEX 0	  // Adjust according to desired video capture device.
#define AUDIO_OUTPUT_DEVICE_INDEX 0
#define SPEAKER_OUPUT_MASK SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT

int main()
{
  IMFMediaSource* pAudioSource = NULL;
  IMFSourceReader* pAudioReader = NULL;
  IMFMediaSink* pAudioSink = NULL;
  IMFStreamSink* pStreamSink = NULL;
  IMFSinkWriter* pAudioSinkWriter = NULL;
  IMFAttributes* pAudioSinkAttributes = NULL;
  IMFMediaTypeHandler* pSinkMediaTypeHandler = NULL, * pSourceMediaTypeHandler = NULL;
  IMFMediaType* pAudioSourceOutputType = NULL, *pAudioSinkInputType = NULL;
  IMFPresentationDescriptor* pSourcePresentationDescriptor = NULL;
  IMFStreamDescriptor* pSourceStreamDescriptor = NULL;
  BOOL fSelected = false;
  
  CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
    "COM initialisation failed.");

  CHECK_HR(MFStartup(MF_VERSION),
    "Media Foundation initialisation failed.");

  /*CHECK_HR(ListAudioOutputDevices(),
    "Failed to list audio output devices.");*/

  //CHECK_HR(ListCaptureDevices(DeviceType::Audio),
  //  "Error listing audio capture devices.");

  // ----- Set up Audio sink (Streaming Audio Renderer). -----

  //CHECK_HR(MFCreateAudioRenderer(NULL, &pAudioSink),
  // "Failed to create streaming audio sink.");

  CHECK_HR(GetAudioOutputDevice(AUDIO_OUTPUT_DEVICE_INDEX, &pAudioSink),
    "Failed to create streaming audio sink.");

  CHECK_HR(pAudioSink->GetStreamSinkByIndex(0, &pStreamSink),
    "Failed to get audio sink stream by index.");

  CHECK_HR(pStreamSink->GetMediaTypeHandler(&pSinkMediaTypeHandler),
    "Failed to get media type handler for stream sink.");

  //CHECK_HR(ListMediaTypes(pSinkMediaTypeHandler),
   // "Failed to list sink media types.");

  CHECK_HR(MFCreateSinkWriterFromMediaSink(pAudioSink, NULL, &pAudioSinkWriter),
    "Failed to create sink writer from audio sink.");

  // ----- Set up audio capture (microphone) source. -----

  CHECK_HR(GetSourceFromCaptureDevice(DeviceType::Audio, AUDIO_CAPTURE_DEVICE_INDEX, &pAudioSource, &pAudioReader),
    "Failed to get microphone source. reader");

  CHECK_HR(pAudioReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pAudioSourceOutputType),
    "Error retrieving current media type from first audio stream.");

  CHECK_HR(pAudioReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE),
    "Failed to set the first audio stream on the source reader.");

  CHECK_HR(pAudioSource->CreatePresentationDescriptor(&pSourcePresentationDescriptor),
    "Failed to create the presentation descriptor from the media source.");

  CHECK_HR(pSourcePresentationDescriptor->GetStreamDescriptorByIndex(0, &fSelected, &pSourceStreamDescriptor),
    "Failed to get source stream descriptor from presentation descriptor.");

  CHECK_HR(pSourceStreamDescriptor->GetMediaTypeHandler(&pSourceMediaTypeHandler),
    "Failed to get source media type handler.");

  DWORD srcMediaTypeCount = 0;
  CHECK_HR(pSourceMediaTypeHandler->GetMediaTypeCount(&srcMediaTypeCount),
    "Failed to get source media type count.");

  std::cout << "Source media type count: " << srcMediaTypeCount << ", is first stream selected " << fSelected << "." << std::endl;
  std::cout << "Default output media type for source reader:" << std::endl;
  std::cout << GetMediaTypeDescription(pAudioSourceOutputType) << std::endl << std::endl;

  // ----- Create a compatible media type and set on the source and sink. -----

  // Set the audio input type on the SAR sink.
  CHECK_HR(MFCreateMediaType(&pAudioSinkInputType), "Failed to create audio sink media type.");
  CHECK_HR(pAudioSourceOutputType->CopyAllItems(pAudioSinkInputType), "Failed to copy media type attributes to sink input type.");
  CHECK_HR(pAudioSinkInputType->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, SPEAKER_OUPUT_MASK), "Failed to set audio channel mask on sink input type.");

  CHECK_HR(pSinkMediaTypeHandler->SetCurrentMediaType(pAudioSinkInputType),
    "Failed to set output media type on sink.");

  std::cout << "Audio sink input media type defined as:" << std::endl;
  std::cout << GetMediaTypeDescription(pAudioSinkInputType) << std::endl << std::endl;

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

      //CHECK_HR(pStreamSink->ProcessSample(pAudioSample), "Stream sink process sample failed.");
      CHECK_HR(pAudioSinkWriter->WriteSample(0, pAudioSample), "Sink writer write sample failed.");
    }

    SAFE_RELEASE(pAudioSample);
  }

done:

  printf("finished.\n");
  auto c = getchar();

  SAFE_RELEASE(pStreamSink);
  SAFE_RELEASE(pAudioSinkWriter);
  SAFE_RELEASE(pSinkMediaTypeHandler);
  SAFE_RELEASE(pSourceMediaTypeHandler);
  SAFE_RELEASE(pSourcePresentationDescriptor);
  SAFE_RELEASE(pSourceStreamDescriptor);
  SAFE_RELEASE(pAudioSourceOutputType);
  SAFE_RELEASE(pAudioSinkInputType);

  return 0;
}

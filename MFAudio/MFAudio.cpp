/******************************************************************************
* Filename: MFAudio.cpp
*
* Description:
* This file contains a C++ console application that plays the audio stream from
* a sample file using the Windows Media Foundation API and the Streaming 
* Audio Renderer:
* https://msdn.microsoft.com/en-us/library/windows/desktop/aa369729%28v=vs.85%29.aspx.
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 01 Jan 2015	Aaron Clauson	  Created, Hobart, Australia.
* 03 Jan 2019 Aaron Clauson   Revisited to get sample working.
*
* License: Public Domain (no warranty, use at own risk)
/******************************************************************************/

#include "../Common/MFUtility.h"

#include <stdio.h>
#include <tchar.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mmdeviceapi.h>
#include <mferror.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#define MEDIA_FILE_PATH L"../MediaFiles/Macroform_-_Simplicity.mp3"
#define AUDIO_DEVICE_INDEX 0 // Select the first audio rendering device returned by the system.

int main()
{
  IMFSourceReader* pSourceReader = NULL;
  IMFMediaType* pFileAudioMediaType = NULL;
  IMFMediaSink* pAudioSink = NULL;
  IMFStreamSink* pStreamSink = NULL;
  IMFMediaTypeHandler* pSinkMediaTypeHandler = NULL;
  IMFMediaType* pSinkSupportedType = NULL;
  IMFMediaType* pSinkMediaType = NULL;
  IMFSinkWriter* pSinkWriter = NULL;
  DWORD sinkMediaTypeCount = 0;

  CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
    "COM initialisation failed.");

  CHECK_HR(MFStartup(MF_VERSION),
    "Media Foundation initialisation failed.");

  ListAudioOutputDevices();

  // Source.
  CHECK_HR(MFCreateSourceReaderFromURL(
    MEDIA_FILE_PATH,
    NULL,
    &pSourceReader),
    "Failed to create source reader from file.");

  CHECK_HR(pSourceReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pFileAudioMediaType),
    "Error retrieving current media type from first audio stream.");

  CHECK_HR(pSourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE),
    "Failed to set the first audio stream on the source reader.");

  std::cout << GetMediaTypeDescription(pFileAudioMediaType) << std::endl;

  // Sink.
  CHECK_HR(GetAudioOutputDevice(AUDIO_DEVICE_INDEX, &pAudioSink),
    "Failed to get audio renderer device.");

  CHECK_HR(pAudioSink->GetStreamSinkByIndex(0, &pStreamSink),
    "Failed to get audio renderer stream by index.");

  CHECK_HR(pStreamSink->GetMediaTypeHandler(&pSinkMediaTypeHandler),
    "Failed to get media type handler.");

  CHECK_HR(pSinkMediaTypeHandler->GetMediaTypeCount(&sinkMediaTypeCount),
    "Error getting sink media type count.");

  // Find a media type that the stream sink supports.
  for (UINT i = 0; i < sinkMediaTypeCount; i++)
  {
    CHECK_HR(pSinkMediaTypeHandler->GetMediaTypeByIndex(i, &pSinkSupportedType),
      "Error getting media type from sink media type handler.");

    std::cout << GetMediaTypeDescription(pSinkSupportedType) << std::endl;

    if (pSinkMediaTypeHandler->IsMediaTypeSupported(pSinkSupportedType, NULL) == S_OK) {
      std::cout << "Matching media type found." << std::endl;
      break;
    }
    else {
      std::cout << "Sink media type does not match." << std::endl;
      SAFE_RELEASE(pSinkSupportedType);
    }
  }

  if (pSinkSupportedType != NULL) {
    // Set the supported type on the reader.
    CHECK_HR(pSourceReader->SetCurrentMediaType(0, NULL, pSinkSupportedType),
      "Failed to set media type on reader.");

    CHECK_HR(MFCreateSinkWriterFromMediaSink(pAudioSink, NULL, &pSinkWriter),
      "Failed to create sink writer for default speaker.");

    CHECK_HR(pSinkWriter->SetInputMediaType(0, pSinkSupportedType, NULL),
      "Error setting sink media type.");

    // Start the read-write loop.
    std::cout << "Read audio samples from file and write to speaker." << std::endl;

    CHECK_HR(pSinkWriter->BeginWriting(),
      "Sink writer begin writing call failed.");

    while (true)
    {
      IMFSample* audioSample = NULL;
      DWORD streamIndex, flags;
      LONGLONG llAudioTimeStamp;

      CHECK_HR(pSourceReader->ReadSample(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        0,                              // Flags.
        &streamIndex,                   // Receives the actual stream index. 
        &flags,                         // Receives status flags.
        &llAudioTimeStamp,              // Receives the time stamp.
        &audioSample                    // Receives the sample or NULL.
      ), "Error reading audio sample.");

      if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
      {
        printf("\tEnd of stream");
        break;
      }
      if (flags & MF_SOURCE_READERF_NEWSTREAM)
      {
        printf("\tNew stream\n");
        break;
      }
      if (flags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)
      {
        printf("\tNative type changed\n");
        break;
      }
      if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
      {
        printf("\tCurrent type changed\n");
        break;
      }
      if (flags & MF_SOURCE_READERF_STREAMTICK)
      {
        printf("Stream tick.\n");
        CHECK_HR(pSinkWriter->SendStreamTick(0, llAudioTimeStamp),
          "Error sending stream tick.");
      }

      if (!audioSample)
      {
        printf("Null audio sample.\n");
      }
      else
      {
        CHECK_HR(pSinkWriter->WriteSample(0, audioSample),
          "The stream sink writer was not happy with the sample.");
      }
    }
  }
  else {
    printf("No matching media type could be found.\n");
  }

done:

  printf("finished.\n");
  int c = getchar();

  SAFE_RELEASE(pSourceReader);
  SAFE_RELEASE(pFileAudioMediaType);
  SAFE_RELEASE(pAudioSink);
  SAFE_RELEASE(pStreamSink);
  SAFE_RELEASE(pSinkMediaTypeHandler);
  SAFE_RELEASE(pSinkSupportedType);
  SAFE_RELEASE(pSinkMediaType);
  SAFE_RELEASE(pSinkWriter);

  return 0;
}

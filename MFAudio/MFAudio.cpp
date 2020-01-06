/******************************************************************************
* Filename: MFAudio.cpp
* 
* Description:
* This file contains a C++ console application that plays the audio stream from
* a sample file using the Windows Media Foundation API. Specifically it's 
* attempting to use the Streaming Audio Renderer 
* (https://msdn.microsoft.com/en-us/library/windows/desktop/aa369729%28v=vs.85%29.aspx).
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 01 Jan 2015	  Aaron Clauson	  Created, Hobart, Australia.
* 03 Jan 2019   Aaron Clauson   Revisited to get sample working.
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
#include <Functiondiscoverykeys_devpkey.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid")

#define MEDIA_FILE_PATH L"../MediaFiles/Macroform_-_Simplicity.mp3"
#define AUDIO_DEVICE_INDEX 0 // Select the first audio rendering device returned by the system.

// Forward function definitions.
HRESULT GetAudioDevice(UINT nDevice, IMFMediaSink** pSink);
HRESULT ListAudioOutputDevices();

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
  CHECK_HR(GetAudioDevice(AUDIO_DEVICE_INDEX, &pAudioSink),
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

/**
* Attempts to get an audio output device sink.
* @param[in] nDevice: the audio device index to attempt to get the sink for.
* @param[out] pSink: the audio device sink.
* @@Returns S_OK if successful or an error code if not.
*/ 
HRESULT GetAudioDevice(UINT nDevice, IMFMediaSink** pSink)
{
  HRESULT hr = S_OK;

  IMMDeviceEnumerator* pEnum = NULL;      // Audio device enumerator.
  IMMDeviceCollection* pDevices = NULL;   // Audio device collection.
  IMMDevice* pDevice = NULL;              // An audio device.
  IMFAttributes* pAttributes = NULL;      // Attribute store.

  LPWSTR wstrID = NULL;                   // Device ID.

  // Create the device enumerator.
  hr = CoCreateInstance(
    __uuidof(MMDeviceEnumerator),
    NULL,
    CLSCTX_ALL,
    __uuidof(IMMDeviceEnumerator),
    (void**)&pEnum
  );

  // Enumerate the rendering devices.
  if (SUCCEEDED(hr))
  {
    hr = pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices);
  }

  // Get ID of the first device in the list.
  if (SUCCEEDED(hr))
  {
    hr = pDevices->Item(nDevice, &pDevice);
  }

  if (SUCCEEDED(hr))
  {
    hr = pDevice->GetId(&wstrID);
  }

  // Create an attribute store and set the device ID attribute.
  if (SUCCEEDED(hr))
  {
    hr = MFCreateAttributes(&pAttributes, 1);
  }

  if (SUCCEEDED(hr))
  {
    hr = pAttributes->SetString(
      MF_AUDIO_RENDERER_ATTRIBUTE_ENDPOINT_ID,
      wstrID
    );
  }

  // Create the audio renderer.
  if (SUCCEEDED(hr))
  {
    hr = MFCreateAudioRenderer(pAttributes, pSink);
  }

  wprintf(L"Selected audio device ID %s.\n", wstrID);

  SAFE_RELEASE(pEnum);
  SAFE_RELEASE(pDevices);
  SAFE_RELEASE(pDevice);
  SAFE_RELEASE(pAttributes);
  CoTaskMemFree(wstrID);

  return hr;
}

/**
* Prints out a list of the aduio output (rendering) devices available.
* @@Returns S_OK if successful or an error code if not.
*
* Remarks:
* See https://docs.microsoft.com/en-us/windows/win32/coreaudio/device-properties.
*/
HRESULT ListAudioOutputDevices()
{
  IMMDeviceEnumerator* pEnum = NULL;      // Audio device enumerator.
  IMMDeviceCollection* pDevices = NULL;   // Audio device collection.
  IMMDevice* pAudioDev = NULL;
  IPropertyStore* pProps = NULL;

  LPWSTR pwstrID = NULL;                   // Device ID.
  UINT audioDeviceCount = 0;

  // Create the device enumerator.
  CHECK_HR(CoCreateInstance(
    __uuidof(MMDeviceEnumerator),
    NULL,
    CLSCTX_ALL,
    __uuidof(IMMDeviceEnumerator),
    (void**)&pEnum),
    "Failed to create device enumerator.");

  // Enumerate the rendering devices.
  CHECK_HR(pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices),
    "Failed to enumerate audio endpoints.");

  CHECK_HR(pDevices->GetCount(&audioDeviceCount),
    "Failed to get the audio device count.");

  wprintf(L"Audio device count %d.\n", audioDeviceCount);

  for (UINT i = 0; i < audioDeviceCount; i++) {
    CHECK_HR(pDevices->Item(i, &pAudioDev),
      "Failed to get audio device pointer.");

    CHECK_HR(pAudioDev->OpenPropertyStore(STGM_READ, &pProps),
      "Failed to open audio device property store.");

    CHECK_HR(pAudioDev->GetId(&pwstrID),
      "Failed to get audio device ID.");

    PROPVARIANT varName;
    PropVariantInit(&varName);

    CHECK_HR(pProps->GetValue(PKEY_Device_FriendlyName, &varName),
      "Failed to get audio device friendly name");

    // Print endpoint friendly name and endpoint ID.
    printf("Endpoint %d: \"%S\" (%S)\n", i, varName.pwszVal, pwstrID);
  }

done:

  SAFE_RELEASE(pEnum);
  SAFE_RELEASE(pDevices);
  SAFE_RELEASE(pAudioDev);
  SAFE_RELEASE(pProps);
  CoTaskMemFree(pwstrID);

  return S_OK;
}
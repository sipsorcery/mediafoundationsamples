/******************************************************************************
* Filename: MFUtility.h
*
* Description:
* This header file contains common macros and functions that are used in the Media Foundation
* sample applications.
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 07 Mar 2015	  Aaron Clauson	  Created, Hobart, Australia.
* 03 Jan 2019   Aaron Clauson   Removed managed C++ references.
*
* License: Public Domain (no warranty, use at own risk)
/******************************************************************************/

#include <stdio.h>
#include <tchar.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <mmdeviceapi.h>
#include <wmcodecdsp.h>
#include <wmsdkidl.h>

#include <codecvt>
#include <iostream>
#include <locale>
#include <string>

#define CHECK_HR(hr, msg) if (hr != S_OK) { printf(msg); printf(" Error: %.2X.\n", hr); goto done; }

#define CHECKHR_GOTO(x, y) if(FAILED(x)) goto y

#define INTERNAL_GUID_TO_STRING( _Attribute, _skip ) \
if (Attr == _Attribute) \
{ \
	pAttrStr = #_Attribute; \
	C_ASSERT((sizeof(#_Attribute) / sizeof(#_Attribute[0])) > _skip); \
	pAttrStr += _skip; \
	goto done; \
} \

template <class T> void SAFE_RELEASE(T** ppT)
{
  if (*ppT)
  {
    (*ppT)->Release();
    *ppT = NULL;
  }
}

template <class T> inline void SAFE_RELEASE(T*& pT)
{
  if (pT != NULL)
  {
    pT->Release();
    pT = NULL;
  }
}

enum class DeviceType { Audio, Video };

/**
* Helper function to get a user friendly description for a Media Foundation
* related GUID.
* @param[in] Attr: the GUID to get a description for.
* @@Returns If the GUID is recognised a user friendly string otherwise NULL.
*/
LPCSTR STRING_FROM_GUID(GUID Attr)
{
  LPCSTR pAttrStr = NULL;

  // Generics
  INTERNAL_GUID_TO_STRING(MF_MT_MAJOR_TYPE, 6);                     // MAJOR_TYPE
  INTERNAL_GUID_TO_STRING(MF_MT_SUBTYPE, 6);                        // SUBTYPE
  INTERNAL_GUID_TO_STRING(MF_MT_ALL_SAMPLES_INDEPENDENT, 6);        // ALL_SAMPLES_INDEPENDENT   
  INTERNAL_GUID_TO_STRING(MF_MT_FIXED_SIZE_SAMPLES, 6);             // FIXED_SIZE_SAMPLES
  INTERNAL_GUID_TO_STRING(MF_MT_COMPRESSED, 6);                     // COMPRESSED
  INTERNAL_GUID_TO_STRING(MF_MT_SAMPLE_SIZE, 6);                    // SAMPLE_SIZE
  INTERNAL_GUID_TO_STRING(MF_MT_USER_DATA, 6);                      // MF_MT_USER_DATA

  // Audio
  INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_NUM_CHANNELS, 12);            // NUM_CHANNELS
  INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_SAMPLES_PER_SECOND, 12);      // SAMPLES_PER_SECOND
  INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 12);    // AVG_BYTES_PER_SECOND
  INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_BLOCK_ALIGNMENT, 12);         // BLOCK_ALIGNMENT
  INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_BITS_PER_SAMPLE, 12);         // BITS_PER_SAMPLE
  INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_VALID_BITS_PER_SAMPLE, 12);   // VALID_BITS_PER_SAMPLE
  INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_SAMPLES_PER_BLOCK, 12);       // SAMPLES_PER_BLOCK
  INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_CHANNEL_MASK, 12);            // CHANNEL_MASK
  INTERNAL_GUID_TO_STRING(MF_MT_AUDIO_PREFER_WAVEFORMATEX, 12);     // PREFER_WAVEFORMATEX

  // Video
  INTERNAL_GUID_TO_STRING(MF_MT_FRAME_SIZE, 6);                     // FRAME_SIZE
  INTERNAL_GUID_TO_STRING(MF_MT_FRAME_RATE, 6);                     // FRAME_RATE
  INTERNAL_GUID_TO_STRING(MF_MT_PIXEL_ASPECT_RATIO, 6);             // PIXEL_ASPECT_RATIO
  INTERNAL_GUID_TO_STRING(MF_MT_INTERLACE_MODE, 6);                 // INTERLACE_MODE
  INTERNAL_GUID_TO_STRING(MF_MT_AVG_BITRATE, 6);                    // AVG_BITRATE
  INTERNAL_GUID_TO_STRING(MF_MT_DEFAULT_STRIDE, 6);									// STRIDE
  INTERNAL_GUID_TO_STRING(MF_MT_AVG_BIT_ERROR_RATE, 6);
  INTERNAL_GUID_TO_STRING(MF_MT_GEOMETRIC_APERTURE, 6);
  INTERNAL_GUID_TO_STRING(MF_MT_MINIMUM_DISPLAY_APERTURE, 6);
  INTERNAL_GUID_TO_STRING(MF_MT_PAN_SCAN_APERTURE, 6);
  INTERNAL_GUID_TO_STRING(MF_MT_VIDEO_NOMINAL_RANGE, 6);

  // Major type values
  INTERNAL_GUID_TO_STRING(MFMediaType_Default, 12);                 // Default
  INTERNAL_GUID_TO_STRING(MFMediaType_Audio, 12);                   // Audio
  INTERNAL_GUID_TO_STRING(MFMediaType_Video, 12);                   // Video
  INTERNAL_GUID_TO_STRING(MFMediaType_Script, 12);                  // Script
  INTERNAL_GUID_TO_STRING(MFMediaType_Image, 12);                   // Image
  INTERNAL_GUID_TO_STRING(MFMediaType_HTML, 12);                    // HTML
  INTERNAL_GUID_TO_STRING(MFMediaType_Binary, 12);                  // Binary
  INTERNAL_GUID_TO_STRING(MFMediaType_SAMI, 12);                    // SAMI
  INTERNAL_GUID_TO_STRING(MFMediaType_Protected, 12);               // Protected

  // Minor video type values
  INTERNAL_GUID_TO_STRING(MFVideoFormat_Base, 14);                  // Base
  INTERNAL_GUID_TO_STRING(MFVideoFormat_MP43, 14);                  // MP43
  INTERNAL_GUID_TO_STRING(MFVideoFormat_WMV1, 14);                  // WMV1
  INTERNAL_GUID_TO_STRING(MFVideoFormat_WMV2, 14);                  // WMV2
  INTERNAL_GUID_TO_STRING(MFVideoFormat_WMV3, 14);                  // WMV3
  INTERNAL_GUID_TO_STRING(MFVideoFormat_MPG1, 14);                  // MPG1
  INTERNAL_GUID_TO_STRING(MFVideoFormat_MPG2, 14);                  // MPG2
  INTERNAL_GUID_TO_STRING(MFVideoFormat_RGB24, 14);									// RGB24
  INTERNAL_GUID_TO_STRING(MFVideoFormat_RGB32, 14);									// RGB32
  INTERNAL_GUID_TO_STRING(MFVideoFormat_H264, 14);									// H264

  // Minor audio type values
  INTERNAL_GUID_TO_STRING(MFAudioFormat_Base, 14);                  // Base
  INTERNAL_GUID_TO_STRING(MFAudioFormat_PCM, 14);                   // PCM
  INTERNAL_GUID_TO_STRING(MFAudioFormat_DTS, 14);                   // DTS
  INTERNAL_GUID_TO_STRING(MFAudioFormat_Dolby_AC3_SPDIF, 14);       // Dolby_AC3_SPDIF
  INTERNAL_GUID_TO_STRING(MFAudioFormat_Float, 14);                 // IEEEFloat
  INTERNAL_GUID_TO_STRING(MFAudioFormat_WMAudioV8, 14);             // WMAudioV8
  INTERNAL_GUID_TO_STRING(MFAudioFormat_WMAudioV9, 14);             // WMAudioV9
  INTERNAL_GUID_TO_STRING(MFAudioFormat_WMAudio_Lossless, 14);      // WMAudio_Lossless
  INTERNAL_GUID_TO_STRING(MFAudioFormat_WMASPDIF, 14);              // WMASPDIF
  INTERNAL_GUID_TO_STRING(MFAudioFormat_MP3, 14);                   // MP3
  INTERNAL_GUID_TO_STRING(MFAudioFormat_MPEG, 14);                  // MPEG

  // Media sub types
  INTERNAL_GUID_TO_STRING(WMMEDIASUBTYPE_I420, 15);                  // I420
  INTERNAL_GUID_TO_STRING(WMMEDIASUBTYPE_WVC1, 0);
  INTERNAL_GUID_TO_STRING(WMMEDIASUBTYPE_WMAudioV8, 0);

  // MP4 Media Subtypes.
  INTERNAL_GUID_TO_STRING(MF_MT_MPEG4_SAMPLE_DESCRIPTION, 6);
  INTERNAL_GUID_TO_STRING(MF_MT_MPEG4_CURRENT_SAMPLE_ENTRY, 6);
  //INTERNAL_GUID_TO_STRING(MFMPEG4Format_MP4A, 0);

done:
  return pAttrStr;
}

/**
* Helper function to get a user friendly description for a media type.
* Note that there may be properties missing or incorrectly described.
* @param[in] pMediaType: pointer to the media type to get a description for.
* @@Returns A string describing the media type.
*/
std::string GetMediaTypeDescription(IMFMediaType* pMediaType)
{
  HRESULT hr = S_OK;
  GUID MajorType;
  UINT32 cAttrCount;
  LPCSTR pszGuidStr;
  std::string description;
  WCHAR TempBuf[200];

  if (pMediaType == NULL)
  {
    description = "<NULL>";
    goto done;
  }

  hr = pMediaType->GetMajorType(&MajorType);
  CHECKHR_GOTO(hr, done);

  pszGuidStr = STRING_FROM_GUID(MajorType);
  if (pszGuidStr != NULL)
  {
    description += pszGuidStr;
    description += ": ";
  }
  else
  {
    description += "Other: ";
  }

  hr = pMediaType->GetCount(&cAttrCount);
  CHECKHR_GOTO(hr, done);

  for (UINT32 i = 0; i < cAttrCount; i++)
  {
    GUID guidId;
    MF_ATTRIBUTE_TYPE attrType;

    hr = pMediaType->GetItemByIndex(i, &guidId, NULL);
    CHECKHR_GOTO(hr, done);

    hr = pMediaType->GetItemType(guidId, &attrType);
    CHECKHR_GOTO(hr, done);

    pszGuidStr = STRING_FROM_GUID(guidId);
    if (pszGuidStr != NULL)
    {
      description += pszGuidStr;
    }
    else
    {
      LPOLESTR guidStr = NULL;

      CHECKHR_GOTO(StringFromCLSID(guidId, &guidStr), done);
      auto wGuidStr = std::wstring(guidStr);
      description += std::string(wGuidStr.begin(), wGuidStr.end()); // GUID's won't have wide chars.

      CoTaskMemFree(guidStr);
    }

    description += "=";

    switch (attrType)
    {
    case MF_ATTRIBUTE_UINT32:
    {
      UINT32 Val;
      hr = pMediaType->GetUINT32(guidId, &Val);
      CHECKHR_GOTO(hr, done);

      description += std::to_string(Val);
      break;
    }
    case MF_ATTRIBUTE_UINT64:
    {
      UINT64 Val;
      hr = pMediaType->GetUINT64(guidId, &Val);
      CHECKHR_GOTO(hr, done);

      if (guidId == MF_MT_FRAME_SIZE)
      {
        //tempStr.Format("W %u, H: %u", HI32(Val), LO32(Val));
        //description += String::Format("W:{0} H:{1}", HI32(Val), LO32(Val));
        description += "W:" + std::to_string(HI32(Val)) + " H: " + std::to_string(LO32(Val));
      }
      else if ((guidId == MF_MT_FRAME_RATE) || (guidId == MF_MT_PIXEL_ASPECT_RATIO))
      {
        //tempStr.Format("W %u, H: %u", HI32(Val), LO32(Val));
        //description += String::Format("W:{0} H:{1}", HI32(Val), LO32(Val));
        description += "W:" + std::to_string(HI32(Val)) + " H: " + std::to_string(LO32(Val));
      }
      else
      {
        //tempStr.Format("%ld", Val);
        description += std::to_string(Val);
      }

      //description += tempStr;

      break;
    }
    case MF_ATTRIBUTE_DOUBLE:
    {
      DOUBLE Val;
      hr = pMediaType->GetDouble(guidId, &Val);
      CHECKHR_GOTO(hr, done);

      //tempStr.Format("%f", Val);
      description += std::to_string(Val);
      break;
    }
    case MF_ATTRIBUTE_GUID:
    {
      GUID Val;
      const char* pValStr;

      hr = pMediaType->GetGUID(guidId, &Val);
      CHECKHR_GOTO(hr, done);

      pValStr = STRING_FROM_GUID(Val);
      if (pValStr != NULL)
      {
        description += pValStr;
      }
      else
      {
        LPOLESTR guidStr = NULL;
        CHECKHR_GOTO(StringFromCLSID(Val, &guidStr), done);
        auto wGuidStr = std::wstring(guidStr);
        description += std::string(wGuidStr.begin(), wGuidStr.end()); // GUID's won't have wide chars.

        CoTaskMemFree(guidStr);
      }

      break;
    }
    case MF_ATTRIBUTE_STRING:
    {
      hr = pMediaType->GetString(guidId, TempBuf, sizeof(TempBuf) / sizeof(TempBuf[0]), NULL);
      if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
      {
        description += "<Too Long>";
        break;
      }
      CHECKHR_GOTO(hr, done);
      auto wstr = std::wstring(TempBuf);
      description += std::string(wstr.begin(), wstr.end()); // It's unlikely the attribute descriptions will contain multi byte chars.

      break;
    }
    case MF_ATTRIBUTE_BLOB:
    {
      description += "<BLOB>";
      break;
    }
    case MF_ATTRIBUTE_IUNKNOWN:
    {
      description += "<UNK>";
      break;
    }
    }

    description += ", ";
  }

done:

  return description;
}

/*
* List all the media modes available on a media source.
* @param[in] pReader: pointer to the media source reader to list the media types for.
*/
void ListModes(IMFSourceReader* pReader)
{
  HRESULT hr = NULL;
  DWORD dwMediaTypeIndex = 0;

  while (SUCCEEDED(hr))
  {
    IMFMediaType* pType = NULL;
    hr = pReader->GetNativeMediaType(0, dwMediaTypeIndex, &pType);
    if (hr == MF_E_NO_MORE_TYPES)
    {
      hr = S_OK;
      break;
    }
    else if (SUCCEEDED(hr))
    {
      // Examine the media type. (Not shown.)
      std::cout << GetMediaTypeDescription(pType) << std::endl;

      pType->Release();
    }
    ++dwMediaTypeIndex;
  }
}

/**
* Prints out a list of the audio or video capture devices available.
* @param[in] deviceType: whether to list audio or video capture devices.
* @@Returns S_OK if successful or an error code if not.
*
* Remarks:
* See https://docs.microsoft.com/en-us/windows/win32/coreaudio/device-properties.
*/
HRESULT ListCaptureDevices(DeviceType deviceType)
{
  IMFAttributes* pDeviceAttributes = NULL;
  IMFActivate** ppDevices = NULL;
  UINT32 deviceCount = 0;

  HRESULT hr = S_OK;

  hr = MFCreateAttributes(&pDeviceAttributes, 1);
  CHECK_HR(hr, "Error creating device attribtues.");

  if (deviceType == DeviceType::Audio) {
    // Request audio capture devices.
    hr = pDeviceAttributes->SetGUID(
      MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
      MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
    CHECK_HR(hr, "Error initialising audio configuration object.");
  }
  else {
    // Request video capture devices.
    hr = pDeviceAttributes->SetGUID(
      MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
      MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    CHECK_HR(hr, "Error initialising video configuration object.");
  }

  hr = MFEnumDeviceSources(pDeviceAttributes, &ppDevices, &deviceCount);
  CHECK_HR(hr, "Error enumerating devices.");

  wprintf(L"Device count %d.\n", deviceCount);

  for (UINT i = 0; i < deviceCount; i++) {

    LPWSTR friendlyName = NULL;
    UINT32 friendlyNameLength = 0;
    IMFMediaSource* pMediaSource = NULL;
    IMFSourceReader* pSourceReader = NULL;

    hr = ppDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &friendlyName, &friendlyNameLength);
    CHECK_HR(hr, "Error retrieving device friendly name.");

    wprintf(L"Device name: %s\n", friendlyName);

    hr = ppDevices[i]->ActivateObject(IID_PPV_ARGS(&pMediaSource));
    CHECK_HR(hr, "Error activating device media source.");

    hr = MFCreateSourceReaderFromMediaSource(
      pMediaSource,
      NULL,
      &pSourceReader);
    CHECK_HR(hr, "Error creating device source reader.");

    ListModes(pSourceReader);

    CoTaskMemFree(friendlyName);
    SAFE_RELEASE(pMediaSource);
    SAFE_RELEASE(pSourceReader);
  }

done:

  SAFE_RELEASE(pDeviceAttributes);
  CoTaskMemFree(ppDevices);

  return hr;
}

/**
* Copies a media type attribute from an input media type to an output media type. Useful when setting
* up the video sink and where a number of the video sink input attributes need to be duplicated on the
* video writer attributes.
* @param[in] pSrc: the media attribute the copy of the key is being made from.
* @param[in] pDest: the media attribute the copy of the key is being made to.
* @param[in] key: the media attribute key to copy.
*/
HRESULT CopyAttribute(IMFAttributes* pSrc, IMFAttributes* pDest, const GUID& key)
{
  PROPVARIANT var;
  PropVariantInit(&var);

  HRESULT hr = S_OK;

  hr = pSrc->GetItem(key, &var);
  if (SUCCEEDED(hr))
  {
    hr = pDest->SetItem(key, var);
  }

  PropVariantClear(&var);
  return hr;
}

/**
* Creates a bitmap file and writes to disk.
* @param[in] fileName: the path to save the file at.
* @param[in] width: the width of the bitmap.
* @param[in] height: the height of the bitmap.
* @param[in] bitsPerPixel: colour depth of the bitmap pixels (typically 24 or 32).
* @param[in] bitmapData: a pointer to the bytes containing the bitmap data.
* @param[in] bitmapDataLength: the number of pixels in the bitmap.
*/
void CreateBitmapFile(LPCWSTR fileName, long width, long height, WORD bitsPerPixel, BYTE* bitmapData, DWORD bitmapDataLength)
{
  HANDLE file;
  BITMAPFILEHEADER fileHeader;
  BITMAPINFOHEADER fileInfo;
  DWORD writePosn = 0;

  file = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);  //Sets up the new bmp to be written to

  fileHeader.bfType = 19778;                                                                    //Sets our type to BM or bmp
  fileHeader.bfSize = sizeof(fileHeader.bfOffBits) + sizeof(RGBTRIPLE);                         //Sets the size equal to the size of the header struct
  fileHeader.bfReserved1 = 0;                                                                   //sets the reserves to 0
  fileHeader.bfReserved2 = 0;
  fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);											//Sets offbits equal to the size of file and info header
  fileInfo.biSize = sizeof(BITMAPINFOHEADER);
  fileInfo.biWidth = width;
  fileInfo.biHeight = height;
  fileInfo.biPlanes = 1;
  fileInfo.biBitCount = bitsPerPixel;
  fileInfo.biCompression = BI_RGB;
  fileInfo.biSizeImage = width * height * (bitsPerPixel / 8);
  fileInfo.biXPelsPerMeter = 2400;
  fileInfo.biYPelsPerMeter = 2400;
  fileInfo.biClrImportant = 0;
  fileInfo.biClrUsed = 0;

  WriteFile(file, &fileHeader, sizeof(fileHeader), &writePosn, NULL);

  WriteFile(file, &fileInfo, sizeof(fileInfo), &writePosn, NULL);

  WriteFile(file, bitmapData, bitmapDataLength, &writePosn, NULL);

  CloseHandle(file);
}
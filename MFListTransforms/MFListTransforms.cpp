/******************************************************************************
* Filename: MFListTransforms.cpp
*
* Description:
* This file contains a C++ console application that attempts to list the Media 
* Foundation Transforms (MFT) available on the system.
* See https://docs.microsoft.com/en-us/windows/win32/medfound/registering-and-enumerating-mfts.
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 26 Feb 2015	  Aaron Clauson	  Created, Hobart, Australia.
* 04 Jan 2020   Aaron Clauson   Tidied up and got it doing something useful.
*
* License: Public Domain (no warranty, use at own risk)
/******************************************************************************/

#include "../Common/MFUtility.h"

#include <stdio.h>
#include <tchar.h>
#include <evr.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <wmcodecdsp.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

void ListTansforms(MFT_REGISTER_TYPE_INFO* pInput, MFT_REGISTER_TYPE_INFO* pOutput);
HRESULT DisplayMFT(IMFActivate* pMFActivate);

int _tmain(int argc, _TCHAR* argv[])
{
	CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
		"COM initialisation failed.");

	CHECK_HR(MFStartup(MF_VERSION),
		"Media Foundation initialisation failed.");

  // Audio subtype GUIDS : https://docs.microsoft.com/en-us/windows/win32/medfound/audio-subtype-guids
  // Video subtype GUIDS : https://docs.microsoft.com/en-us/windows/win32/medfound/video-subtype-guids

  MFT_REGISTER_TYPE_INFO videoYuv = { MFMediaType_Video, MFVideoFormat_YUY2 };
  MFT_REGISTER_TYPE_INFO videoRgb24 = { MFMediaType_Video, MFVideoFormat_RGB24 };
  MFT_REGISTER_TYPE_INFO videoH264 = { MFMediaType_Video, MFVideoFormat_H264 };
  MFT_REGISTER_TYPE_INFO videoVP8 = { MFMediaType_Video, MFVideoFormat_VP80 };
	MFT_REGISTER_TYPE_INFO audioPcm = { MFMediaType_Audio, MFAudioFormat_PCM };
	MFT_REGISTER_TYPE_INFO audioMp3 = { MFMediaType_Audio, MFAudioFormat_MP3 };

  std::cout << "Video MFT's for input: YUV2, output: all" << std::endl;
  ListTansforms(&videoYuv, NULL);
  std::cout << std::endl;

  std::cout << "Video MFT's for input: YUV2, output: H264" << std::endl;
  ListTansforms(&videoYuv, &videoH264);
  std::cout << std::endl;

  std::cout << "Video MFT's for input: YUV2, output: VP8" << std::endl;
  ListTansforms(&videoYuv, &videoVP8);
  std::cout << std::endl;

  std::cout << "Video MFT's for input: RGB24, output: H264" << std::endl;
  ListTansforms(&videoRgb24, &videoH264);
  std::cout << std::endl;

  std::cout << "Audio MFT's for input: PCM, output: all" << std::endl;
  ListTansforms(&audioPcm, NULL);
  std::cout << std::endl;

  std::cout << "Audio MFT's for input: PCM, output: MP3" << std::endl;
  ListTansforms(&audioPcm, &audioMp3);
  std::cout << std::endl;

done:
	printf("finished.\n");
	auto c = getchar();

	return 0;
}

/**
* Prints out a list of all the Media Foundation Transforms that match for
* the input media type and the optional output media type.
* @param[in] pInput: pointer to the MFT input type to match.
* @param[in] pOutput: Optional. Pointer to the MFT output type to match. If
                      NULL all MFT's for the input type will be listed.
* Remarks:
* Copied from https://github.com/mvaneerde/blog/blob/develop/mftenum/mftenum/mftenum.cpp.
*/
void ListTansforms(MFT_REGISTER_TYPE_INFO* pInput, MFT_REGISTER_TYPE_INFO* pOutput)
{
  IMFActivate** ppActivate = NULL;
  IMFTransform* pDecoder = NULL;
  UINT32 mftCount = 0;
  
  auto category = (pInput->guidMajorType == MFMediaType_Audio) ? MFT_CATEGORY_AUDIO_ENCODER : MFT_CATEGORY_VIDEO_ENCODER;
  CHECK_HR(MFTEnumEx(category,
    NULL, //MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT | MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
    pInput,
    pOutput,
    &ppActivate,
    &mftCount
  ), "MFTEnumEx failed.");

  printf("MFT count %d.\n", mftCount);

  for (int i = 0; i < mftCount; i++) {
    auto hr = DisplayMFT(ppActivate[i]);
  }

done:

  for (UINT32 i = 0; i < mftCount; i++)
  {
    ppActivate[i]->Release();
  }
  CoTaskMemFree(ppActivate);
}

/**
* Prints out the friendly name for a Media Foundation Transform.
*
* Remarks:
* Copied from https://github.com/mvaneerde/blog/blob/develop/mftenum/mftenum/mftenum.cpp.
*/
HRESULT DisplayMFT(IMFActivate* pMFActivate) {
  HRESULT hr;

  // get the CLSID GUID from the IMFAttributes of the activation object
  GUID guidMFT = { 0 };
  hr = pMFActivate->GetGUID(MFT_TRANSFORM_CLSID_Attribute, &guidMFT);
  if (MF_E_ATTRIBUTENOTFOUND == hr) {
    std::cout << "IMFTransform has no CLSID." << std::endl;
    return hr;
  }
  else if (FAILED(hr)) {
    std::cerr << "IMFAttributes::GetGUID(MFT_TRANSFORM_CLSID_Attribute) failed: hr = " << hr << std::endl;
    return hr;
  }

  LPWSTR szGuid = NULL;
  hr = StringFromIID(guidMFT, &szGuid);
  if (FAILED(hr)) {
    std::cerr << "StringFromIID failed: hr = " << hr << std::endl;
    return hr;
  }

  // get the friendly name string from the IMFAttributes of the activation object
  LPWSTR szFriendlyName = NULL;
  UINT len = 0;
  hr = pMFActivate->GetAllocatedString(
    MFT_FRIENDLY_NAME_Attribute,
    &szFriendlyName,
    &len
  );

  if (MF_E_ATTRIBUTENOTFOUND == hr) {
    std::cout << "IMFTransform has no friendly name." << std::endl;
    return hr;
  }
  else if (FAILED(hr)) {
    std::cerr << "IMFAttributes::GetAllocatedString(MFT_FRIENDLY_NAME_Attribute) failed: hr = " << hr << std::endl;
    return hr;
  }
  //CoTaskMemFreeOnExit freeFriendlyName(szFriendlyName);

  std::wcout << szFriendlyName << " (" << szGuid << ")" << std::endl;

  CoTaskMemFree(szGuid);
  CoTaskMemFree(szFriendlyName);

  return S_OK;
}
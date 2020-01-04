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
* 04 Jan 2020   Aaron Clauson   Tidied up and got ti doing something useful.
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

HRESULT DisplayMFT(IMFActivate* pMFActivate);

int _tmain(int argc, _TCHAR* argv[])
{
	CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
		"COM initialisation failed.");

	CHECK_HR(MFStartup(MF_VERSION),
		"Media Foundation initialisation failed.");

	IMFActivate** ppActivate = NULL;
	IMFTransform* pDecoder = NULL;
	UINT32 mftCount = 0;

	MFT_REGISTER_TYPE_INFO info = { 0 };
	info.guidMajorType = MFMediaType_Video;
	info.guidSubtype = MFVideoFormat_YUY2;

	MFT_REGISTER_TYPE_INFO outInfo = { 0 };
	outInfo.guidMajorType = MFMediaType_Video;
	outInfo.guidSubtype = MFVideoFormat_H264;

	CHECK_HR(MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER,
		MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT | MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
		&info,      // Input type
		&outInfo,   // Output type
		&ppActivate,
		&mftCount
		), "MFTEnumEx failed.");

	printf("MFT count %d.\n", mftCount);

	for (int i = 0; i < mftCount; i++) {

		//CHECK_HR(ppActivate[i]->ActivateObject(IID_PPV_ARGS(&pDecoder)),
		//	"Failed to activate decoder.");
    auto hr = DisplayMFT(ppActivate[i]);
	}

done:

	for (UINT32 i = 0; i < mftCount; i++)
	{
		ppActivate[i]->Release();
	}
	CoTaskMemFree(ppActivate);

	printf("finished.\n");
	auto c = getchar();

	return 0;
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
#pragma prefast(suppress: __WARNING_PASSING_FUNCTION_UNEXPECTED_NULL, "IMFAttributes::GetAllocatedString third argument is optional");
  hr = pMFActivate->GetAllocatedString(
    MFT_FRIENDLY_NAME_Attribute,
    &szFriendlyName,
    NULL // don't care about the length of the string, and MSDN agrees, but SAL annotation is missing "opt"
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
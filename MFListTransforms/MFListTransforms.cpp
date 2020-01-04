/******************************************************************************
* Filename: MFListTransforms.cpp
*
* Description:
* This file contains a C++ console application that attempts to list the Media 
* Foundation Transforms (MFT) available on the system.
*
* Status: Not Working.
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 26 Feb 2015	  Aaron Clauson	  Created, Hobart, Australia.
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

int _tmain(int argc, _TCHAR* argv[])
{
	CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
		"COM initialisation failed.");

	CHECK_HR(MFStartup(MF_VERSION),
		"Media Foundation initialisation failed.");

	CLSID *ppCLSIDs = NULL;
	UINT32 mftCount = 0;

	MFT_REGISTER_TYPE_INFO info = { 0 };
	info.guidMajorType = MFMediaType_Video;
	info.guidSubtype = MFVideoFormat_YUY2;

	MFT_REGISTER_TYPE_INFO outInfo = { 0 };
	outInfo.guidMajorType = MFMediaType_Video;
	outInfo.guidSubtype = MFVideoFormat_H264;

	CHECK_HR(MFTEnum(MFT_CATEGORY_VIDEO_ENCODER,
		0,          // Reserved
		&info,      // Input type
		&outInfo,   // Output type
		NULL,       // Reserved
		&ppCLSIDs,
		&mftCount
		), "MFTEnum failed.");



done:

	printf("finished.\n");
	auto c = getchar();

	return 0;
}

/// Filename: MFListTransforms.cpp
///
/// Description:
/// This file contains a C++ console application that attempts to list the Media Foundation Transforms (MFT) available on
/// the system.
///
/// Note: This sample is currently not working.
///
/// History:
/// 26 Feb 2015	Aaron Clauson (aaron@sipsorcery.com)	Created.
///
/// License: Public

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

#define CHECK_HR(hr, msg) if (hr != S_OK) { printf(msg); printf("Error: %.2X.\n", hr); goto done; }

int _tmain(int argc, _TCHAR* argv[])
{
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	MFStartup(MF_VERSION);

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
		&info,       // Input type
		&outInfo,      // Output type
		NULL,       // Reserved
		&ppCLSIDs,
		&mftCount
		), "MFTEnum failed.\n");

done:

	printf("finished.\n");
	getchar();

	return 0;
}

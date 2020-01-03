/// Filename: MFCaptureRawFramesToFile.cpp
///
/// Description:
/// This file contains a C++ console application (a few managed CLR calls are used to make life easier) that captures 
/// individual frames from a webcam and dumps them in binary format to an output file.
///
/// To convert the raw yuv data dumped at the end of this sample use the ffmpeg commands below:
/// ffmpeg -vcodec rawvideo -s 640x480 -pix_fmt yuv420p -i rawframes.yuv -vframes 1 out.jpeg
/// ffmpeg -vcodec rawvideo -s 640x480 -pix_fmt yuv420p -i rawframes.yuv out.avi
///
/// More info see : https://ffmpeg.org/ffmpeg.html#Video-and-Audio-file-format-conversion
///
/// Note: The webcam index and the source reader media output type will need adjustment depending on the
/// the configuration of video devices on any machine running this sample.
///
/// History:
/// 06 Mar 2015	Aaron Clauson (aaron@sipsorcery.com)	Created.
///
/// License: Public

#include <stdio.h>
#include <tchar.h>
#include <evr.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include <fstream>
#include "../Common/MFUtility.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

using namespace System;
using namespace System::IO;
using namespace System::Runtime::InteropServices;

int main(array<System::String ^> ^args)
{
	const int WEBCAM_DEVICE_INDEX = 0;	// <--- Set to 0 to use default system webcam.
	const int SAMPLE_COUNT = 100;

	std::ofstream outputBuffer("rawframes.yuv", std::ios::out | std::ios::binary);

	IMFMediaSource *videoSource = NULL;
	UINT32 videoDeviceCount = 0;
	IMFAttributes *videoConfig = NULL;
	IMFActivate **videoDevices = NULL;
	IMFSourceReader *videoReader = NULL;
	WCHAR *webcamFriendlyName;
	IMFMediaType*videoSourceOutputType = NULL;
	IMFMediaType *pSrcOutMediaType = NULL;

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	MFStartup(MF_VERSION);

	// Get the first available webcam.
	CHECK_HR(MFCreateAttributes(&videoConfig, 1), "Error creating video configuation.\n");

	// Request video capture devices.
	CHECK_HR(videoConfig->SetGUID(
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID), "Error initialising video configuration object.");

	CHECK_HR(MFEnumDeviceSources(videoConfig, &videoDevices, &videoDeviceCount), "Error enumerating video devices.\n");

	CHECK_HR(videoDevices[WEBCAM_DEVICE_INDEX]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &webcamFriendlyName, NULL), "Error retrieving vide device friendly name.\n");

	wprintf(L"First available webcam: %s\n", webcamFriendlyName);

	CHECK_HR(videoDevices[WEBCAM_DEVICE_INDEX]->ActivateObject(IID_PPV_ARGS(&videoSource)), "Error activating video device.\n");

	// Create a source reader.
	CHECK_HR(MFCreateSourceReaderFromMediaSource(
		videoSource,
		videoConfig,
		&videoReader), "Error creating video source reader.\n");

	// Note the webcam needs to support this media type. The list of media types supported can be obtained using the ListTypes function in MFUtility.h.
	MFCreateMediaType(&pSrcOutMediaType);
	pSrcOutMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	pSrcOutMediaType->SetGUID(MF_MT_SUBTYPE, WMMEDIASUBTYPE_I420);
	MFSetAttributeSize(pSrcOutMediaType, MF_MT_FRAME_SIZE, 640, 480);

	CHECK_HR(videoReader->SetCurrentMediaType(0, NULL, pSrcOutMediaType), "Failed to set media type on source reader.\n");

	printf("Reading video samples from webcam.\n");

	IMFSample *videoSample = NULL;
	DWORD streamIndex, flags;
	LONGLONG llVideoTimeStamp, llSampleDuration;
	int sampleCount = 0;

	while (sampleCount <= SAMPLE_COUNT)
	{
		CHECK_HR(videoReader->ReadSample(
			MF_SOURCE_READER_FIRST_VIDEO_STREAM,
			0,                              // Flags.
			&streamIndex,                   // Receives the actual stream index. 
			&flags,                         // Receives status flags.
			&llVideoTimeStamp,              // Receives the time stamp.
			&videoSample                    // Receives the sample or NULL.
			), "Error reading video sample.");

		if (flags & MF_SOURCE_READERF_STREAMTICK)
		{
			printf("Stream tick.\n");
		}

		if (videoSample)
		{
			printf("Writing sample %i.\n", sampleCount);

			CHECK_HR(videoSample->SetSampleTime(llVideoTimeStamp), "Error setting the video sample time.\n");
			CHECK_HR(videoSample->GetSampleDuration(&llSampleDuration), "Error getting video sample duration.\n");

			IMFMediaBuffer *buf = NULL;
			DWORD bufLength;
			CHECK_HR(videoSample->ConvertToContiguousBuffer(&buf), "ConvertToContiguousBuffer failed.\n");
			CHECK_HR(buf->GetCurrentLength(&bufLength), "Get buffer length failed.\n");

			printf("Sample length %i.\n", bufLength);

			byte *byteBuffer;
			DWORD buffCurrLen = 0;
			DWORD buffMaxLen = 0;
			buf->Lock(&byteBuffer, &buffMaxLen, &buffCurrLen);
			
			outputBuffer.write((char *)byteBuffer, bufLength);
			
			buf->Release();
		}

		sampleCount++;
	}

	outputBuffer.close();

done:

	printf("finished.\n");
	getchar();

	return 0;
}

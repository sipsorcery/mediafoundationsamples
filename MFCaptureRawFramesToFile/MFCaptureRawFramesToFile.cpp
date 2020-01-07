/******************************************************************************
* Filename: MFCaptureRawFramesToFile.cpp
*
* Description:
* This file contains a C++ console application that captures individual frames 
* from a webcam and dumps them in binary format to an output file.
*
* To convert the raw yuv data dumped at the end of this sample use the ffmpeg 
* commands below:
* ffmpeg -vcodec rawvideo -s 640x480 -pix_fmt yuv420p -i rawframes.yuv -vframes 1 out.jpeg
* ffmpeg -vcodec rawvideo -s 640x480 -pix_fmt yuv420p -i rawframes.yuv out.avi
*
* More info see: https://ffmpeg.org/ffmpeg.html#Video-and-Audio-file-format-conversion
*
* Note: The webcam index and the source reader media output type will need 
* adjustment depending on the the configuration of video devices on the machine 
* running this sample.
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 06 Mar 2015		Aaron Clauson (aaron@sipsorcery.com)	Created.
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
#include <mferror.h>
#include <wmcodecdsp.h>
#include <fstream>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

#define WEBCAM_DEVICE_INDEX 0	// Adjust according to desired video capture device.
#define SAMPLE_COUNT 100			// Adjust depending on number of samples to capture.
#define CAPTURE_FILENAME "rawframes.yuv"
#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480

int main()
{
	IMFMediaSource* videoSource = NULL;
	UINT32 videoDeviceCount = 0;
	IMFAttributes* videoConfig = NULL;
	IMFActivate** videoDevices = NULL;
	IMFSourceReader* videoReader = NULL;
	WCHAR* webcamFriendlyName = NULL;
	IMFMediaType* videoSourceOutputType = NULL;
	IMFMediaType* pSrcOutMediaType = NULL;
	UINT webcamNameLength = 0;

	std::ofstream outputBuffer(CAPTURE_FILENAME, std::ios::out | std::ios::binary);

	CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
		"COM initialisation failed.");

	CHECK_HR(MFStartup(MF_VERSION),
		"Media Foundation initialisation failed.");

	// Get the first available webcam.
	CHECK_HR(MFCreateAttributes(&videoConfig, 1), 
		"Error creating video configuation.");

	// Request video capture devices.
	CHECK_HR(videoConfig->SetGUID(
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID), 
		"Error initialising video configuration object.");

	CHECK_HR(MFEnumDeviceSources(videoConfig, &videoDevices, &videoDeviceCount), 
		"Error enumerating video devices.");

	CHECK_HR(videoDevices[WEBCAM_DEVICE_INDEX]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &webcamFriendlyName, &webcamNameLength),
		"Error retrieving video device friendly name.");

	wprintf(L"First available webcam: %s\n", webcamFriendlyName);

	CHECK_HR(videoDevices[WEBCAM_DEVICE_INDEX]->ActivateObject(IID_PPV_ARGS(&videoSource)), 
		"Error activating video device.");

	// Create a source reader.
	CHECK_HR(MFCreateSourceReaderFromMediaSource(
		videoSource,
		videoConfig,
		&videoReader), 
		"Error creating video source reader.");

	// The list of media types supported by the webcam.
	//ListModes(videoReader);

	/*CHECK_HR(videoReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &videoSourceOutputType),
		"Error retrieving current media type from first video stream.");*/

	// Note the webcam needs to support this media type.
	CHECK_HR(MFCreateMediaType(&pSrcOutMediaType), "Failed to create media type.");
	CHECK_HR(pSrcOutMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set video media type.");
	CHECK_HR(pSrcOutMediaType->SetGUID(MF_MT_SUBTYPE, WMMEDIASUBTYPE_I420), "Failed to set video media sub type to I420.");
	CHECK_HR(MFSetAttributeSize(pSrcOutMediaType, MF_MT_FRAME_SIZE, FRAME_WIDTH, FRAME_HEIGHT), "Failed to set frame size.");
	//CHECK_HR(CopyAttribute(videoSourceOutputType, pSrcOutMediaType, MF_MT_DEFAULT_STRIDE), "Failed to copy default stride attribute.");

	CHECK_HR(videoReader->SetCurrentMediaType(0, NULL, pSrcOutMediaType), 
		"Failed to set media type on source reader.");

	printf("Reading video samples from webcam.");

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

			CHECK_HR(videoSample->SetSampleTime(llVideoTimeStamp), "Error setting the video sample time.");
			CHECK_HR(videoSample->GetSampleDuration(&llSampleDuration), "Error getting video sample duration.");

			IMFMediaBuffer *buf = NULL;
			DWORD bufLength;
			CHECK_HR(videoSample->ConvertToContiguousBuffer(&buf), "ConvertToContiguousBuffer failed.");
			CHECK_HR(buf->GetCurrentLength(&bufLength), "Get buffer length failed.");

			printf("Sample length %i.\n", bufLength);

			byte *byteBuffer;
			DWORD buffCurrLen = 0;
			DWORD buffMaxLen = 0;
			CHECK_HR(buf->Lock(&byteBuffer, &buffMaxLen, &buffCurrLen), "Failed to lock video sample buffer.");
			
			outputBuffer.write((char *)byteBuffer, bufLength);
			
			CHECK_HR(buf->Unlock(), "Failed to unlock video sample buffer.");

			buf->Release();
			videoSample->Release();
		}

		sampleCount++;
	}

	outputBuffer.close();

done:

	printf("finished.\n");
	int c = getchar();

	SAFE_RELEASE(videoSource);
	SAFE_RELEASE(videoConfig);
	SAFE_RELEASE(videoDevices);
	SAFE_RELEASE(videoReader);
	SAFE_RELEASE(videoSourceOutputType);
	SAFE_RELEASE(pSrcOutMediaType);

	return 0;
}

/******************************************************************************
* Filename: MFWebCamToFile.cpp
*
* Description:
* This file contains a C++ console application that captures the real-time video 
* stream from a webcam to an MP4 file.
*
* Note: The webcam index and the source reader media output type will need
* adjustment depending on the configuration of video devices on the machine
* running this sample.
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 26 Feb 2015		Aaron Clauson	Created, Hobart, Australia.
* 10 Jan 2020		Aaron Clauson	Added defines for webcam resolution.
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
#include <codecapi.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

#define WEBCAM_DEVICE_INDEX 0	// Adjust according to desired video capture device.
#define SAMPLE_COUNT 100			// Adjust depending on number of samples to capture.
#define CAPTURE_FILENAME L"capture.mp4"
#define OUTPUT_FRAME_WIDTH 640		// Adjust if the webcam does not support this frame width.
#define OUTPUT_FRAME_HEIGHT 480		// Adjust if the webcam does not support this frame height.
#define OUTPUT_FRAME_RATE 30      // Adjust if the webcam does not support this frame rate.

int main()
{
	IMFMediaSource *pVideoSource = NULL;
	UINT32 videoDeviceCount = 0;
	IMFAttributes *videoConfig = NULL;
	IMFActivate **videoDevices = NULL;
	IMFSourceReader *pVideoReader = NULL;
	WCHAR *webcamFriendlyName;
	IMFMediaType* pSourceOutputType = NULL;
	IMFSinkWriter *pWriter = NULL;
	IMFMediaType *pVideoOutType = NULL;
	DWORD writerVideoStreamIndex = 0;
	UINT webcamNameLength = 0;

	CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
		"COM initialisation failed.");

	CHECK_HR(MFStartup(MF_VERSION),
		"Media Foundation initialisation failed.");

	// Get the first available webcam.
	CHECK_HR(MFCreateAttributes(&videoConfig, 1),
		"Error creating video configuration.");

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

	CHECK_HR(videoDevices[WEBCAM_DEVICE_INDEX]->ActivateObject(IID_PPV_ARGS(&pVideoSource)), 
		"Error activating video device.");

	// Create a source reader.
	CHECK_HR(MFCreateSourceReaderFromMediaSource(
		pVideoSource,
		videoConfig,
		&pVideoReader), 
		"Error creating video source reader.");

	// Note the webcam needs to support this media type. 
	MFCreateMediaType(&pSourceOutputType);
	CHECK_HR(pSourceOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set major video type.");
	CHECK_HR(pSourceOutputType->SetGUID(MF_MT_SUBTYPE, WMMEDIASUBTYPE_I420), "Failed to set video sub type to I420.");
	//CHECK_HR(pSourceOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24), "Failed to set video sub type.");
	CHECK_HR(MFSetAttributeRatio(pSourceOutputType, MF_MT_FRAME_RATE, OUTPUT_FRAME_RATE, 1), "Failed to set frame rate on source reader out type.");
	CHECK_HR(MFSetAttributeSize(pSourceOutputType, MF_MT_FRAME_SIZE, OUTPUT_FRAME_WIDTH, OUTPUT_FRAME_HEIGHT), "Failed to set frame size.");

	CHECK_HR(pVideoReader->SetCurrentMediaType(0, NULL, pSourceOutputType),
		"Failed to set media type on source reader.");

	// Create the MP4 sink writer.
	CHECK_HR(MFCreateSinkWriterFromURL(
		CAPTURE_FILENAME,
		NULL,
		NULL,
		&pWriter), 
		"Error creating mp4 sink writer.");

	CHECK_HR(MFTRegisterLocalByCLSID(
		__uuidof(CColorConvertDMO),
		MFT_CATEGORY_VIDEO_PROCESSOR,
		L"",
		MFT_ENUM_FLAG_SYNCMFT,
		0,
		NULL,
		0,
		NULL
		), 
		"Error registering colour converter DSP.\n");

	// Configure the output video type on the sink writer.
	CHECK_HR(MFCreateMediaType(&pVideoOutType), "Configure encoder failed to create media type for video output sink.");
	CHECK_HR(pSourceOutputType->CopyAllItems(pVideoOutType), "Error copying media type attributes from source output media type.");
	// Only thing we want to change from source to sink is to get an mp4 output.
	CHECK_HR(pVideoOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264), "Failed to set video writer attribute, video format (H.264).");
	CHECK_HR(pVideoOutType->SetUINT32(MF_MT_AVG_BITRATE, 240000), "Error setting average bit rate.");
	CHECK_HR(pVideoOutType->SetUINT32(MF_MT_INTERLACE_MODE, 2), "Error setting interlace mode.");
	CHECK_HR(MFSetAttributeRatio(pVideoOutType, MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Base, 1), "Failed to set profile on H264 MFT out type.");

	CHECK_HR(pWriter->AddStream(pVideoOutType, &writerVideoStreamIndex), 
		"Failed to add the video stream to the sink writer.");
	
	CHECK_HR(pWriter->SetInputMediaType(writerVideoStreamIndex, pSourceOutputType, NULL),
		"Error setting the sink writer video input type.");

	CHECK_HR(pWriter->BeginWriting(), 
		"Failed to begin writing on the H.264 sink.");

	DWORD streamIndex, flags;
	LONGLONG llVideoTimeStamp;
	IMFSample *videoSample = NULL;
	LONGLONG llVideoBaseTime = 0;
	int sampleCount = 0;

	printf("Recording...\n");

	while (sampleCount < SAMPLE_COUNT)
	{
		CHECK_HR(pVideoReader->ReadSample(
			MF_SOURCE_READER_ANY_STREAM,				// Stream index.
			0,																	// Flags.
			&streamIndex,												// Receives the actual stream index. 
			&flags,															// Receives status flags.
			&llVideoTimeStamp,									// Receives the time stamp.
			&videoSample												// Receives the sample or NULL.
			), "Error reading video sample.");

		if (videoSample)
		{
			// Re-base the time stamp.
			llVideoTimeStamp -= llVideoBaseTime;

			CHECK_HR(videoSample->SetSampleTime(llVideoTimeStamp), "Set video sample time failed.");
			CHECK_HR(pWriter->WriteSample(writerVideoStreamIndex, videoSample), "Write video sample failed.");

			SAFE_RELEASE(&videoSample);
		}

		sampleCount++;
	}

	printf("Finalising the capture.\n");

	if (pWriter)
	{
		CHECK_HR(pWriter->Finalize(), "Error finalising H.264 sink writer.");
	}


done:

	printf("finished.\n");
	auto c = getchar();

	SAFE_RELEASE(pVideoSource);
	SAFE_RELEASE(videoConfig);
	SAFE_RELEASE(videoDevices);
	SAFE_RELEASE(pVideoReader);
	SAFE_RELEASE(pVideoOutType);
	SAFE_RELEASE(pSourceOutputType);
	SAFE_RELEASE(pWriter);

	return 0;
}

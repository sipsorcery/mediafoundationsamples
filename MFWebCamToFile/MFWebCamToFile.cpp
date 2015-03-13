/// Filename: MFWebCamToFile.cpp
///
/// Description:
/// This file contains a C++ console application that captures the realtime video stream from a webcam to an MP4 file.
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

HRESULT CopyAttribute(IMFAttributes *pSrc, IMFAttributes *pDest, const GUID& key);

int _tmain(int argc, _TCHAR* argv[])
{
	const int WEBCAM_DEVICE_INDEX = 1;	// <--- Set to 0 to use default system webcam.
	const WCHAR *CAPTURE_FILENAME = L"sample.mp4";

	IMFMediaSource *videoSource = NULL;
	UINT32 videoDeviceCount = 0;
	IMFAttributes *videoConfig = NULL;
	IMFActivate **videoDevices = NULL;
	IMFSourceReader *videoReader = NULL;
	WCHAR *webcamFriendlyName;
	IMFMediaType*videoSourceOutputType = NULL;
	IMFSinkWriter *pWriter;
	IMFMediaType *pVideoOutType = NULL;
	DWORD writerVideoStreamIndex = 0;

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

	CHECK_HR(videoReader->GetCurrentMediaType(
		(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
		&videoSourceOutputType), "Error retrieving current media type from first video stream.\n");

	// Create the MP4 sink writer.
	CHECK_HR(MFCreateSinkWriterFromURL(
		CAPTURE_FILENAME,
		NULL,
		NULL,
		&pWriter), "Error creating mp4 sink writer.");

	CHECK_HR(MFTRegisterLocalByCLSID(
		__uuidof(CColorConvertDMO),
		MFT_CATEGORY_VIDEO_PROCESSOR,
		L"",
		MFT_ENUM_FLAG_SYNCMFT,
		0,
		NULL,
		0,
		NULL
		), "Error registering colour converter DSP.\n");

	// Configure the output video type on the sink writer.
	CHECK_HR(MFCreateMediaType(&pVideoOutType), "Configure encoder failed to create media type for video output sink.");
	CHECK_HR(pVideoOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set video writer attribute, media type.");
	CHECK_HR(pVideoOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264), "Failed to set video writer attribute, video format (H.264).");
	CHECK_HR(pVideoOutType->SetUINT32(MF_MT_AVG_BITRATE, 240 * 1000), "Failed to set video writer attribute, bit rate.");
	CHECK_HR(CopyAttribute(videoSourceOutputType, pVideoOutType, MF_MT_FRAME_SIZE), "Failed to set video writer attribute, frame size.");
	CHECK_HR(CopyAttribute(videoSourceOutputType, pVideoOutType, MF_MT_FRAME_RATE), "Failed to set video writer attribute, frame rate.");
	CHECK_HR(CopyAttribute(videoSourceOutputType, pVideoOutType, MF_MT_PIXEL_ASPECT_RATIO), "Failed to set video writer attribute, aspect ratio.");
	CHECK_HR(CopyAttribute(videoSourceOutputType, pVideoOutType, MF_MT_INTERLACE_MODE), "Failed to set video writer attribute, interlace mode.");
	
	CHECK_HR(pWriter->AddStream(pVideoOutType, &writerVideoStreamIndex), "Failed to add the video stream to the sink writer.");
	
	pVideoOutType->Release();

	CHECK_HR(pWriter->SetInputMediaType(writerVideoStreamIndex, videoSourceOutputType, NULL), "Error setting the sink writer video input type.\n");

	getchar();

	CHECK_HR(pWriter->BeginWriting(), "Failed to begin writing on the H.264 sink.\n");

	DWORD streamIndex, flags;
	LONGLONG llVideoTimeStamp, llAudioTimeStamp;
	IMFSample *videoSample = NULL, *audioSample = NULL;
	CRITICAL_SECTION critsec;
	BOOL bFirstVideoSample = TRUE, bFirstAudioSample = TRUE;
	LONGLONG llVideoBaseTime = 0, llAudioBaseTime = 0;
	int sampleCount = 0;

	InitializeCriticalSection(&critsec);

	printf("Recording...\n");

	while (sampleCount < 100)
	{
		// Initial read results in a null pSample??
		CHECK_HR(videoReader->ReadSample(
			MF_SOURCE_READER_ANY_STREAM,    // Stream index.
			0,                              // Flags.
			&streamIndex,                   // Receives the actual stream index. 
			&flags,                         // Receives status flags.
			&llVideoTimeStamp,                   // Receives the time stamp.
			&videoSample                        // Receives the sample or NULL.
			), "Error reading video sample.\n");

		//wprintf(L"Video stream %d (%I64d)\n", streamIndex, llVideoTimeStamp);

		if (videoSample)
		{
			if (bFirstVideoSample)
			{
				llVideoBaseTime = llVideoTimeStamp;
				bFirstVideoSample = FALSE;
			}

			// rebase the time stamp
			llVideoTimeStamp -= llVideoBaseTime;

			CHECK_HR(videoSample->SetSampleTime(llVideoTimeStamp), "Set video sample time failed.\n");
			CHECK_HR(pWriter->WriteSample(writerVideoStreamIndex, videoSample), "Write video sample failed.\n");
		}

		sampleCount++;
	}

	printf("Finalising the capture.\n");

	if (pWriter)
	{
		CHECK_HR(pWriter->Finalize(), "Error finalising H.264 sink writer.\n");
	}


done:

	printf("finished.\n");
	getchar();

	return 0;
}

/*
Copies a media type attribute from an input media type to an output media type. Useful when setting
up the video sink and where a number of the video sink input attributes need to be duplicated on the
video writer attributes.
*/
HRESULT CopyAttribute(IMFAttributes *pSrc, IMFAttributes *pDest, const GUID& key)
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
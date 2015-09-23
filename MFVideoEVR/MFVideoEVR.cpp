/// Filename: MFVideoEVR.cpp
///
/// Description:
/// This file contains a C++ console application that is attempting to play the video stream from a sample MP4 file using the Windows
/// Media Foundation API. Specifically it's attempting to use the Enhanced Video Renderer (https://msdn.microsoft.com/en-us/library/windows/desktop/ms694916%28v=vs.85%29.aspx)
/// to playback the video.
///
/// Common HRESULT values https://msdn.microsoft.com/en-us/library/windows/desktop/aa378137(v=vs.85).aspx
/// 0x80004002 E_NOINTERFACE
/// 0xC00D36D7 MF_E_NO_CLOCK
/// 0xC00D36B4 MF_E_INVALIDMEDIATYPE
/// 0xC00D5212 MF_E_TOPO_CODEC_NOT_FOUND
/// 0xC00D36C8 MF_E_NO_SAMPLE_TIMESTAMP
/// 
/// NOTE: This sample is currently not working.
///
/// History:
/// 01 Jan 2015	Aaron Clauson (aaron@sipsorcery.com)	Created.
/// 15 Sep 2015 Aaron Clauson							Trying with webcam instead of file but still no idea how to interface between EVR and an actual window.
///
/// License: Public

#include <stdio.h>
#include <tchar.h>
#include <evr.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include "..\Common\MFUtility.h"

#include <windows.h>
#include <windowsx.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "evr.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "Strmiids")
#pragma comment(lib, "wmcodecdspuuid.lib")

#define CHECK_HR(hr, msg) if (hr != S_OK) { printf(msg); printf("Error: %.2X.\n", hr); goto done; }

void InitializeWindow();

// Constants 
const WCHAR CLASS_NAME[] = L"MFVideoEVR Window Class";
const WCHAR WINDOW_NAME[] = L"MFVideoEVR";
const int WEBCAM_DEVICE_INDEX = 1;

// Globals.
HWND _hwnd;

using namespace System::Threading::Tasks;

int main()
{
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	MFStartup(MF_VERSION);

	IMFMediaSource *videoSource = NULL;
	UINT32 videoDeviceCount = 0;
	IMFAttributes *videoConfig = NULL;
	IMFActivate **videoDevices = NULL;
	IMFSourceReader *videoReader = NULL;
	WCHAR *webcamFriendlyName;
	IMFMediaType *videoSourceOutputType = NULL, *pvideoSourceModType = NULL, *pSrcOutMediaType = NULL;
	IMFSourceResolver *pSourceResolver = NULL;
	IUnknown* uSource = NULL;
	IMFMediaSource *mediaFileSource = NULL;
	IMFAttributes *pVideoReaderAttributes = NULL;
	IMFMediaType *pVideoOutType = NULL;
	MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;
	IMFMediaSink *pVideoSink = NULL;
	IMFStreamSink *pStreamSink = NULL;
	IMFMediaTypeHandler *pMediaTypeHandler = NULL;
	IMFMediaType *pMediaType = NULL;
	IMFMediaType *pSinkMediaType = NULL;
	IMFSinkWriter *pSinkWriter = NULL;
	IMFVideoRenderer *pVideoRenderer = NULL;
	IMFVideoPresenter *pVideoPresenter = nullptr;
	IMFVideoDisplayControl *pVideoDisplayControl = nullptr;
	IMFGetService *pService = nullptr;
	IMFActivate* pActive = NULL;
	MFVideoNormalizedRect nrcDest = { 0.5f, 0.5f, 1.0f, 1.0f };
	IMFPresentationTimeSource *pSystemTimeSource = nullptr;
	IMFMediaType *sinkPreferredType = nullptr;
	IMFPresentationClock *pClock = NULL;
	IMFPresentationTimeSource *pTimeSource = NULL;

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
	
	Task::Factory->StartNew(gcnew Action(InitializeWindow));

	Sleep(1000);

	if (_hwnd == nullptr)
	{
		printf("Failed to initialise video window.\n");
		goto done;
	}

	// Set up the reader for the file.
	CHECK_HR(MFCreateSourceResolver(&pSourceResolver), "MFCreateSourceResolver failed.\n");

	CHECK_HR(pSourceResolver->CreateObjectFromURL(
		L"..\\..\\..\\MediaFiles\\big_buck_bunny.mp4",		// URL of the source.
		MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
		NULL,                       // Optional property store.
		&ObjectType,				// Receives the created object type. 
		&uSource					// Receives a pointer to the media source.
		), "Failed to create media source resolver for file.\n");

	CHECK_HR(uSource->QueryInterface(IID_PPV_ARGS(&mediaFileSource)), "Failed to create media file source.\n");

	CHECK_HR(MFCreateAttributes(&pVideoReaderAttributes, 2), "Failed to create attributes object for video reader.\n");
	CHECK_HR(pVideoReaderAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID), "Failed to set dev source attribute type for reader config.\n");
	CHECK_HR(pVideoReaderAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, 1), "Failed to set enable video processing attribute type for reader config.\n");

	CHECK_HR(MFCreateSourceReaderFromMediaSource(mediaFileSource, pVideoReaderAttributes, &videoReader),
		"Error creating media source reader.\n");

	CHECK_HR(videoReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &videoSourceOutputType),
		"Error retrieving current media type from first video stream.\n");

	Console::WriteLine("Default output media type for source reader:");
	Console::WriteLine(GetMediaTypeDescription(videoSourceOutputType));
	Console::WriteLine();

	// Set the video output type on the source reader.
	CHECK_HR(MFCreateMediaType(&pvideoSourceModType), "Failed to create video output media type.\n");
	CHECK_HR(pvideoSourceModType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set video output media major type.\n");
	CHECK_HR(pvideoSourceModType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32), "Failed to set video sub-type attribute on EVR input media type.\n");
	CHECK_HR(pvideoSourceModType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive), "Failed to set interlace mode attribute on EVR input media type.\n");
	CHECK_HR(pvideoSourceModType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE), "Failed to set independent samples attribute on EVR input media type.\n");
	CHECK_HR(MFSetAttributeRatio(pvideoSourceModType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set pixel aspect ratio attribute on EVR input media type.\n");
	CHECK_HR(CopyAttribute(videoSourceOutputType, pvideoSourceModType, MF_MT_FRAME_SIZE), "Failed to copy video frame size attribute from input file to output sink.\n");
	CHECK_HR(CopyAttribute(videoSourceOutputType, pvideoSourceModType, MF_MT_FRAME_RATE), "Failed to copy video frame rate attribute from input file to output sink.\n");

	CHECK_HR(videoReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pvideoSourceModType), "Failed to set media type on source reader.\n");

	Console::WriteLine("Output media type set on source reader:");
	Console::WriteLine(GetMediaTypeDescription(pvideoSourceModType));
	Console::WriteLine();

	// Create EVR sink .
	//CHECK_HR(MFCreateVideoRenderer(__uuidof(IMFMediaSink), (void**)&pVideoSink), "Failed to create video sink.\n");

	CHECK_HR(MFCreateVideoRendererActivate(_hwnd, &pActive), "Failed to created video rendered activation context.\n");
	CHECK_HR(pActive->ActivateObject(IID_IMFMediaSink, (void**)&pVideoSink), "Failed to activate IMFMediaSink interface on video sink.\n");

	// Initialize the renderer before doing anything else including querying for other interfaces (https://msdn.microsoft.com/en-us/library/windows/desktop/ms704667(v=vs.85).aspx).
	CHECK_HR(pVideoSink->QueryInterface(__uuidof(IMFVideoRenderer), (void**)&pVideoRenderer), "Failed to get video Renderer interface from EVR media sink.\n");
	CHECK_HR(pVideoRenderer->InitializeRenderer(NULL, NULL), "Failed to initialise the video renderer.\n");

	CHECK_HR(pVideoSink->QueryInterface(__uuidof(IMFGetService), (void**)&pService), "Failed to get service interface from EVR media sink.\n");
	CHECK_HR(pService->GetService(MR_VIDEO_RENDER_SERVICE, __uuidof(IMFVideoDisplayControl), (void**)&pVideoDisplayControl), "Failed to get video display control interface from service interface.\n");

	CHECK_HR(pVideoSink->GetStreamSinkByIndex(0, &pStreamSink), "Failed to get video renderer stream by index.\n");
	CHECK_HR(pStreamSink->GetMediaTypeHandler(&pMediaTypeHandler), "Failed to get media type handler.\n");

	// Set the video output type on the source reader.
	CHECK_HR(MFCreateMediaType(&pVideoOutType), "Failed to create video output media type.\n");
	CHECK_HR(pVideoOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set video output media major type.\n");
	CHECK_HR(pVideoOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32), "Failed to set video sub-type attribute on EVR input media type.\n");
	CHECK_HR(pVideoOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive), "Failed to set interlace mode attribute on EVR input media type.\n");
	CHECK_HR(pVideoOutType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE), "Failed to set independent samples attribute on EVR input media type.\n");
	CHECK_HR(MFSetAttributeRatio(pVideoOutType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set pixel aspect ratio attribute on EVR input media type.\n");
	CHECK_HR(CopyAttribute(videoSourceOutputType, pVideoOutType, MF_MT_FRAME_SIZE), "Failed to copy video frame size attribute from input file to output sink.\n");
	CHECK_HR(CopyAttribute(videoSourceOutputType, pVideoOutType, MF_MT_FRAME_RATE), "Failed to copy video frame rate attribute from input file to output sink.\n");

	//CHECK_HR(pMediaTypeHandler->GetMediaTypeByIndex(0, &pSinkMediaType), "Failed to get sink media type.\n");
	CHECK_HR(pMediaTypeHandler->SetCurrentMediaType(pVideoOutType), "Failed to set current media type.\n");

	Console::WriteLine("Input media type set on EVR:");
	Console::WriteLine(GetMediaTypeDescription(pVideoOutType));
	Console::WriteLine();

	CHECK_HR(MFCreatePresentationClock(&pClock), "Failed to create presentation clock.\n");
	CHECK_HR(MFCreateSystemTimeSource(&pTimeSource), "Failed to create system time source.\n");
	CHECK_HR(pClock->SetTimeSource(pTimeSource), "Failed to set time source.\n");
	CHECK_HR(pVideoSink->SetPresentationClock(pClock), "Failed to set presentation clock on video sink.\n");

	Console::WriteLine("Press any key to start video sampling...");
	Console::ReadLine();

	IMFSample *videoSample = NULL;
	DWORD streamIndex, flags;
	LONGLONG llTimeStamp;
	bool isBitmapSaved = false;
	IMFMediaBuffer *pBuffer = nullptr;

	while (true)
	{
		CHECK_HR(videoReader->ReadSample(
			MF_SOURCE_READER_FIRST_VIDEO_STREAM,
			0,                              // Flags.
			&streamIndex,                   // Receives the actual stream index. 
			&flags,                         // Receives status flags.
			&llTimeStamp,					// Receives the time stamp.
			&videoSample                    // Receives the sample or NULL.
			), "Error reading video sample.");

		if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
		{
			printf("End of stream.\n");
			break;
		}
		if (flags & MF_SOURCE_READERF_STREAMTICK)
		{
			printf("Stream tick.\n");
		} 

		if (!videoSample)
		{
			printf("Null video sample.\n");
		}
		else
		{
			printf("Attempting to write sample to stream sink.\n");

			CHECK_HR(videoSample->SetSampleTime(llTimeStamp), "Error setting the video sample time.\n");
			//CHECK_HR(videoSample->SetSampleDuration(33), "Error setting the video sample duration.\n");

			CHECK_HR(pStreamSink->ProcessSample(videoSample), "Streamsink process sample failed.\n");
		}

		SafeRelease(&videoSample);
	}

done:

	printf("finished.\n");
	getchar();

	return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc = 0;

	switch (uMsg)
	{
		//HANDLE_MSG(hwnd, WM_CLOSE, OnClose);
		//HANDLE_MSG(hwnd, WM_KEYDOWN, OnKeyDown);
		//HANDLE_MSG(hwnd, WM_PAINT, OnPaint);
		//HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
		//HANDLE_MSG(hwnd, WM_SIZE, OnSize);

		case WM_PAINT:
			hdc = BeginPaint(_hwnd, &ps);
			EndPaint(_hwnd, &ps);
			break;

		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return 0;
}

void InitializeWindow()
{
	WNDCLASS wc = { 0 };

	wc.lpfnWndProc = WindowProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = CLASS_NAME;

	if (RegisterClass(&wc))
	{
		_hwnd = CreateWindow(
			CLASS_NAME,
			WINDOW_NAME,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			640,
			480,
			NULL,
			NULL,
			GetModuleHandle(NULL),
			NULL
			);

		if (_hwnd)
		{
			ShowWindow(_hwnd, SW_SHOWDEFAULT);

			MSG Msg = { 0 };

			while (GetMessage(&Msg, _hwnd, 0, 0) > 0)
			{
				TranslateMessage(&Msg);
				DispatchMessage(&Msg);
			}
		}
	}
}
/// Filename: MFVideo.cpp
///
/// Description:
/// This file contains a C++ console application that is attempting to play the video stream from a sample MP4 file using the Windows
/// Media Foundation API. Specifically it's attempting to use the Enhanced Video Renderer (https://msdn.microsoft.com/en-us/library/windows/desktop/ms694916%28v=vs.85%29.aspx)
/// to playback the video.
///
/// NOTE: This sample is currently not working.
///
/// History:
/// 01 Jan 2015	Aaron Clauson (aaron@sipsorcery.com)	Created.
///
/// License: Public

#include <stdio.h>
#include <tchar.h>
#include <evr.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include <windows.h>
#include <windowsx.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#define CHECK_HR(hr, msg) if (hr != S_OK) { printf(msg); printf("Error: %.2X.\n", hr); goto done; }

DWORD WINAPI CreateVideoWindow(LPVOID pContext);
BOOL InitializeWindow(HWND *pHwnd);

// Constants 
const WCHAR CLASS_NAME[] = L"MFVideo Window Class";
const WCHAR WINDOW_NAME[] = L"MFVideo";

int _tmain(int argc, _TCHAR* argv[])
{
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	MFStartup(MF_VERSION);

	IMFSourceResolver *pSourceResolver = NULL;
	IUnknown* uSource = NULL;
	IMFMediaSource *mediaFileSource = NULL;
	IMFAttributes *pVideoReaderAttributes = NULL;
	IMFSourceReader *pSourceReader = NULL;
	IMFMediaType *pVideoOutType = NULL;
	IMFMediaType *pFileVideoMediaType = NULL;
	MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;
	IMFMediaSink *pVideoSink = NULL;
	IMFStreamSink *pStreamSink = NULL;
	IMFMediaTypeHandler *pMediaTypeHandler = NULL;
	IMFMediaType *pMediaType = NULL;
	IMFMediaType *pSinkMediaType = NULL;
	IMFSinkWriter *pSinkWriter = NULL;
	IMFVideoRenderer *pVideoRenderer = NULL;

	HANDLE h = CreateThread(NULL, 0, CreateVideoWindow, NULL, 0L, NULL);

	// Set up the reader for the file.
	CHECK_HR(MFCreateSourceResolver(&pSourceResolver), "MFCreateSourceResolver failed.\n");

	CHECK_HR(pSourceResolver->CreateObjectFromURL(
		L"..\\..\\MediaFiles\\big_buck_bunny.mp4",		// URL of the source.
		MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
		NULL,                       // Optional property store.
		&ObjectType,				// Receives the created object type. 
		&uSource					// Receives a pointer to the media source.
		), "Failed to create media source resolver for file.\n");

	CHECK_HR(uSource->QueryInterface(IID_PPV_ARGS(&mediaFileSource)), "Failed to create media file source.\n");

	CHECK_HR(MFCreateAttributes(&pVideoReaderAttributes, 2), "Failed to create attributes object for video reader.\n");

	CHECK_HR(pVideoReaderAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID),
		"Failed to set dev source attribute type for reader config.\n");

	CHECK_HR(pVideoReaderAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, 1), "Failed to set enable video processing attribute type for reader config.\n");

	CHECK_HR(MFCreateSourceReaderFromMediaSource(mediaFileSource, pVideoReaderAttributes, &pSourceReader),
		"Error creating media source reader.\n");

	CHECK_HR(pSourceReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pFileVideoMediaType),
		"Error retrieving current media type from first video stream.\n");

	// Set the video output type on the source reader.
	CHECK_HR(MFCreateMediaType(&pVideoOutType), "Failed to create video output media type.\n");
	CHECK_HR(pVideoOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set video output media major type.\n");
	CHECK_HR(pVideoOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32), "Failed to set video output audio sub type (RGB32).\n");

	CHECK_HR(pSourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pVideoOutType),
		"Error setting reader video output type.\n");
	 
	// Create EVR sink .
	CHECK_HR(MFCreateVideoRenderer(__uuidof(IMFMediaSink), (void**)&pVideoSink), "Failed to create video sink.\n");

	CHECK_HR(pVideoSink->GetStreamSinkByIndex(0, &pStreamSink), "Failed to get video renderer stream by index.\n");

	//CHECK_HR(pStreamSink->GetMediaTypeHandler(&pMediaTypeHandler), "Failed to get media type handler.\n");

	//CHECK_HR(pMediaTypeHandler->GetMediaTypeByIndex(0, &pSinkMediaType), "Failed to get sink media type.\n");

	//CHECK_HR(pMediaTypeHandler->SetCurrentMediaType(pSinkMediaType), "Failed to set current media type.\n");

	// printf("Sink Media Type:\n");
	// Dump pSinkMediaType.

	CHECK_HR(MFCreateSinkWriterFromMediaSink(pVideoSink, NULL, &pSinkWriter), "Failed to create sink writer from video sink.\n");

	printf("Read video samples from file and write to window.\n");

	IMFSample *videoSample = NULL;
	DWORD streamIndex, flags;
	LONGLONG llTimeStamp;

	for (int index = 0; index < 10; index++)
		//while (true)
	{
		// Initial read results in a null pSample??
		CHECK_HR(pSourceReader->ReadSample(
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
			pSinkWriter->SendStreamTick(0, llTimeStamp);
		}

		if (!videoSample)
		{
			printf("Null video sample.\n");
		}
		else
		{
			CHECK_HR(videoSample->SetSampleTime(llTimeStamp), "Error setting the video sample time.\n");

			CHECK_HR(pSinkWriter->WriteSample(0, videoSample), "The stream sink writer was not happy with the sample.\n");
		}
	}

done:

	printf("finished.\n");
	getchar();

	return 0;
}

DWORD WINAPI CreateVideoWindow(LPVOID pContext)
{
	HWND hwnd = 0;
	MSG msg = { 0 };

	if (!InitializeWindow(&hwnd))
	{
		return 0;
	}

	// Message loop
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

void OnPaint(HWND hwnd)
{
	PAINTSTRUCT ps;
	HDC hdc = 0;

	hdc = BeginPaint(hwnd, &ps);

	//if (g_pPlayer && g_bHasVideo)
	//{
	//	// Playback has started and there is video. 

	//	// Do not draw the window background, because the video 
	//	// frame fills the entire client area.

	//	g_pPlayer->UpdateVideo();
	//}
	//else
	//{
	//	// There is no video stream, or playback has not started.
	//	// Paint the entire client area.

	//	FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
	//}

	FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

	EndPaint(hwnd, &ps);
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		//HANDLE_MSG(hwnd, WM_CLOSE, OnClose);
		//HANDLE_MSG(hwnd, WM_KEYDOWN, OnKeyDown);
		HANDLE_MSG(hwnd, WM_PAINT, OnPaint);
		//HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
		//HANDLE_MSG(hwnd, WM_SIZE, OnSize);

	case WM_ERASEBKGND:
		return 1;

	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

BOOL InitializeWindow(HWND *pHwnd)
{
	WNDCLASS wc = { 0 };

	wc.lpfnWndProc = WindowProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = CLASS_NAME;
	//wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);

	if (!RegisterClass(&wc))
	{
		return FALSE;
	}

	HWND hwnd = CreateWindow(
		CLASS_NAME,
		WINDOW_NAME,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		GetModuleHandle(NULL),
		NULL
		);

	if (!hwnd)
	{
		return FALSE;
	}

	ShowWindow(hwnd, SW_SHOWDEFAULT);
	UpdateWindow(hwnd);

	*pHwnd = hwnd;

	return TRUE;
}
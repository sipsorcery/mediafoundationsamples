/// Filename: MFTopology.cpp
///
/// Description:
/// This file contains a C++ console application that plays the audio from a sample MP4 file using the Windows
/// Media Foundation API. Specifically it uses the automatic topology creation approach rather than
/// creating source readers or sink writers. With the topology the playback revolves around the use of a MediaSession
/// object.
///
/// While this approach is relatively succinct compared to the other Media Foundation options it is largely a black
/// box and as far as I can determine it's only useful for playing back media files or perhaps certain RTSP end points.
/// Anything more exotic such as playing back media from an RTP stream - which is my ultimate goal - is likely to 
/// require a different approach.

///
/// History:
/// 01 Jan 2015	Aaron Clauson (aaron@sipsorcery.com)	Created.
/// 17 Sep 2015 Aaron Clauson (aaron@sipsorcery.com)	Added the video playback option in addition to the audio one. Can only get one at a time working so far.
///
/// License: Public

#include <stdio.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

// Macros
#define CHECK_HR(hr, msg) if (hr != S_OK) { printf(msg); printf("Error: %.2X.\n", hr); goto done; }

// Function definitions.
//DWORD InitializeWindow(LPVOID lpThreadParameter);
void InitializeWindow();

// Constants 
const WCHAR CLASS_NAME[] = L"MFVideo Window Class";
const WCHAR WINDOW_NAME[] = L"MFVideo";

// Globals.
HWND _hwnd;

using namespace System;
using namespace System::Threading::Tasks;

int main()
{
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	MFStartup(MF_VERSION);

	IMFSourceResolver* pSourceResolver = NULL;
	IUnknown* uSource = NULL;
	IMFTopology *pTopology = NULL;
	IMFPresentationDescriptor *pSourcePD = NULL;
	IMFStreamDescriptor *pSourceSD = NULL;
	IMFMediaSource *pSource = NULL;
	IMFMediaSession *pSession = NULL;
	MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;
	IMFMediaTypeHandler *pHandler = NULL;
	IMFActivate *pActivate = NULL;
	IMFTopologyNode *pSourceNode = NULL, *pOutputNode = NULL;
	BOOL fSelected = FALSE;
	PROPVARIANT varStart;
	DWORD sourceStreamCount = 0;
	IMFStreamSink *pOutputSink = NULL;
	IMFMediaType *pOutputNodeMediaType = NULL;

	Task::Factory->StartNew(gcnew Action(InitializeWindow));

	// This chews 25% CPU. Need to find out why.
	/*CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)InitializeWindow, NULL, 0, NULL);

	Sleep(1000);

	if (_hwnd == nullptr)
	{
		printf("Failed to initialise video window.\n");
		goto done;
	}*/

	CHECK_HR(MFCreateSourceResolver(&pSourceResolver), "Failed to create source resolved.\n");

	CHECK_HR(pSourceResolver->CreateObjectFromURL(
		L"../MediaFiles/big_buck_bunny.mp4",       // URL of the source.
    MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
		NULL,                       // Optional property store.
		&ObjectType,				// Receives the created object type. 
		&uSource					// Receives a pointer to the media source.
		), "Failed to create media object from URL.\n");

	CHECK_HR(MFCreateMediaSession(NULL, &pSession), "Failed to create media session.\n");

	CHECK_HR(MFCreateTopology(&pTopology), "Failed to create topology object.\n");

	CHECK_HR(uSource->QueryInterface(IID_PPV_ARGS(&pSource)), "Failed to get media source.\n");

	// Add source node to topology.
	CHECK_HR(pSource->CreatePresentationDescriptor(&pSourcePD), "Failed to create presentation descriptor from source.\n");

	// Get the number of streams in the media source.
	CHECK_HR(pSourcePD->GetStreamDescriptorCount(&sourceStreamCount), "Failed to get source stream count.\n");
		
	printf("Source stream count %i.\n", sourceStreamCount); 

	for (DWORD i = 0; i < sourceStreamCount; i++)
	{
		CHECK_HR(pSourcePD->GetStreamDescriptorByIndex(i, &fSelected, &pSourceSD), "Failed to get stream descriptor from presentation descriptor.\n");

		CHECK_HR(pSourceSD->GetMediaTypeHandler(&pHandler), "Failed to create media type handler from presentation descriptor.\n");

		GUID guidMajorType;
		CHECK_HR(pHandler->GetMajorType(&guidMajorType), "Failed to get media type handler from source stream.\n");

		if (guidMajorType == MFMediaType_Audio)
		{
			printf("Creating audio renderer for stream index %i.\n", i);
			CHECK_HR(MFCreateAudioRendererActivate(&pActivate), "Failed to create audio renderer activate object.\n");
			//break;
		}
		else if (guidMajorType == MFMediaType_Video)
		{
			//printf("Ignoring video stream.\n");
			printf("Creating video renderer for stream index %i.\n", i);
			CHECK_HR(MFCreateVideoRendererActivate(_hwnd, &pActivate), "Failed to create video renderer activate object.\n");
			break;
		}
	}

	// Creating and adding source node to topology.
	CHECK_HR(MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pSourceNode), "Failed to create source node.\n");
	CHECK_HR(pSourceNode->SetUnknown(MF_TOPONODE_SOURCE, pSource), "Failed to set top node source on topology node.\n");
	CHECK_HR(pSourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pSourcePD), "Failed to set presentation descriptor on topology node.\n");
	CHECK_HR(pSourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSourceSD), "Failed to set stream descriptor on topology node.\n");

	CHECK_HR(pTopology->AddNode(pSourceNode), "Failed to add source node to topology.\n");

	// Creating and adding output node to topology.
	CHECK_HR(MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pOutputNode), "Failed to create sink node.\n");

	CHECK_HR(pOutputNode->SetObject(pActivate), "Failed to set the activate object on the output node.\n");
	CHECK_HR(pOutputNode->SetUINT32(MF_TOPONODE_STREAMID, 0), "Failed to set the stream ID on the output node.\n");
	CHECK_HR(pOutputNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE), "Failed to set the no shutdown attribute on the output node.\n");

	CHECK_HR(pTopology->AddNode(pOutputNode), "Failed to add output node to topology.\n");

	CHECK_HR(pSourceNode->ConnectOutput(0, pOutputNode, 0), "Failed to connect the source node to the output node.\n");

	CHECK_HR(pSession->SetTopology(0, pTopology), "Failed to set the topology on the session.\n");

	//CHECK_HR(pOutputNode->GetOutputPrefType(0, &pOutputNodeMediaType), "Failed to get the output node preferred media type.\n");
	//CHECK_HR(pOutputNode->QueryInterface(IID_PPV_ARGS(&pOutputNodeMediaType)), "Failed to get an media type from the topology output node.\n");
	//CHECK_HR(pOutputNode->QueryInterface(IID_PPV_ARGS(&pOutputSink)), "Failed to get an out sink from the topology output node.\n");
	//CHECK_HR(pOutputNode->QueryInterface(IID_PPV_ARGS(&pHandler)), "Failed to get an out sink from the topology output node.\n");
	//CHECK_HR(pActivate->QueryInterface(IID_PPV_ARGS(&pOutputNodeMediaType)), "Failed to get the activation object preferred media type.\n");

	printf("Press any key to start the session...\n");
	auto c = getchar();

	PropVariantInit(&varStart);
	CHECK_HR(pSession->Start(&GUID_NULL, &varStart), "Failed to start session.\n");

done:

	printf("finished.\n");
	c = getchar();

	return 0;
}

// Window creation and processing functions for video rendering.

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

//DWORD InitializeWindow(LPVOID lpThreadParameter)
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


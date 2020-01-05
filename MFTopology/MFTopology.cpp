/******************************************************************************
* Filename: MFTopology.cpp
*
* Description:
* This file contains a C++ console application that plays the audio from a sample 
* MP4 file using the Windows Media Foundation API. Specifically it uses the automatic 
* topology creation approach rather than creating source readers or sink writers. 
* With the topology the playback revolves around the use of a MediaSession object.
*
* While this approach is relatively succinct compared to the other Media Foundation 
* options it is largely a black box and as far as I can determine it's only useful 
* for playing back media files or perhaps certain RTSP end points. Anything more 
* exotic such as playing back media from an RTP stream - which is my ultimate goal - 
* is likely to require a different approach.
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 01 Jan 2015	  Aaron Clauson	  Created, Hobart, Australia.
* 17 Sep 2015   Aaron Clauson   Added the video playback option in addition to the 
*                               audio one. Can only get one at a time working so far.
*
* License: Public Domain (no warranty, use at own risk)
/******************************************************************************/

#include "../Common/MFUtility.h"

#include <stdio.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#define MEDIA_FILE_PATH L"../MediaFiles/big_buck_bunny.mp4"

// Forward function definitions.
DWORD InitializeWindow(LPVOID lpThreadParameter);

// Constants 
const WCHAR CLASS_NAME[] = L"MFVideo Window Class";
const WCHAR WINDOW_NAME[] = L"MFVideo";

// Globals.
HWND _hwnd;

int main()
{
	IMFSourceResolver* pSourceResolver = NULL;
	IUnknown* uSource = NULL;
	IMFTopology *pTopology = NULL;
	IMFPresentationDescriptor *pSourcePD = NULL;
	IMFStreamDescriptor *pSourceSD = NULL;
	IMFMediaSource *pSource = NULL;
	IMFMediaSession *pSession = NULL;
	MF_OBJECT_TYPE ObjectType = MF_OBJECT_TYPE::MF_OBJECT_INVALID;
	IMFMediaTypeHandler *pHandler = NULL;
	IMFActivate *pActivate = NULL;
	IMFTopologyNode *pSourceNode = NULL, *pOutputNode = NULL;
	BOOL fSelected = FALSE;
	PROPVARIANT varStart;
	DWORD sourceStreamCount = 0;
	IMFStreamSink *pOutputSink = NULL;
	IMFMediaType *pOutputNodeMediaType = NULL;

	// Create a separate Window and thread to host the Video player.
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)InitializeWindow, NULL, 0, NULL);
	Sleep(1000);
	if (_hwnd == nullptr)
	{
		printf("Failed to initialise video window.\n");
		goto done;
	}

	CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
		"COM initialisation failed.");

	CHECK_HR(MFStartup(MF_VERSION),
		"Media Foundation initialisation failed.");

	CHECK_HR(MFCreateSourceResolver(&pSourceResolver), 
		"Failed to create source resolved.");

	CHECK_HR(pSourceResolver->CreateObjectFromURL(
		MEDIA_FILE_PATH,													// URL of the source.
    MF_RESOLUTION_MEDIASOURCE,								// Create a source object.
		NULL,																			// Optional property store.
		&ObjectType,															// Receives the created object type. 
		&uSource																	// Receives a pointer to the media source.
		), "Failed to create media object from URL.\n");

	CHECK_HR(MFCreateMediaSession(NULL, &pSession), 
		"Failed to create media session.");

	CHECK_HR(MFCreateTopology(&pTopology), 
		"Failed to create topology object.");

	CHECK_HR(uSource->QueryInterface(IID_PPV_ARGS(&pSource)), 
		"Failed to get media source.");

	// Add source node to topology.
	CHECK_HR(pSource->CreatePresentationDescriptor(&pSourcePD), 
		"Failed to create presentation descriptor from source.");

	// Get the number of streams in the media source.
	CHECK_HR(pSourcePD->GetStreamDescriptorCount(&sourceStreamCount), 
		"Failed to get source stream count.");
		
	printf("Source stream count %i.\n", sourceStreamCount); 

	// Iterate over the available source streams and create renderers.
	for (DWORD i = 0; i < sourceStreamCount; i++)
	{
		CHECK_HR(pSourcePD->GetStreamDescriptorByIndex(i, &fSelected, &pSourceSD), 
			"Failed to get stream descriptor from presentation descriptor.");

		CHECK_HR(pSourceSD->GetMediaTypeHandler(&pHandler), 
			"Failed to create media type handler from presentation descriptor.");

		GUID guidMajorType;
		CHECK_HR(pHandler->GetMajorType(&guidMajorType), 
			"Failed to get media type handler from source stream.");

		if (guidMajorType == MFMediaType_Audio)
		{
			printf("Creating audio renderer for stream index %i.\n", i);
			CHECK_HR(MFCreateAudioRendererActivate(&pActivate), 
				"Failed to create audio renderer activate object.");
		}
		else if (guidMajorType == MFMediaType_Video)
		{
			printf("Creating video renderer for stream index %i.\n", i);
			CHECK_HR(MFCreateVideoRendererActivate(_hwnd, &pActivate), 
				"Failed to create video renderer activate object.");
		}
	}

	// Creating and adding source node to topology.
	CHECK_HR(MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pSourceNode), "Failed to create source node.");
	CHECK_HR(pSourceNode->SetUnknown(MF_TOPONODE_SOURCE, pSource), "Failed to set top node source on topology node.");
	CHECK_HR(pSourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pSourcePD), "Failed to set presentation descriptor on topology node.");
	CHECK_HR(pSourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSourceSD), "Failed to set stream descriptor on topology node.");

	CHECK_HR(pTopology->AddNode(pSourceNode), 
		"Failed to add source node to topology.");

	// Creating and adding output node to topology.
	CHECK_HR(MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pOutputNode), 
		"Failed to create sink node.");

	CHECK_HR(pOutputNode->SetObject(pActivate), 
		"Failed to set the activate object on the output node.");

	CHECK_HR(pOutputNode->SetUINT32(MF_TOPONODE_STREAMID, 0), 
		"Failed to set the stream ID on the output node.");

	CHECK_HR(pOutputNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE), 
		"Failed to set the no shutdown attribute on the output node.");

	CHECK_HR(pTopology->AddNode(pOutputNode), 
		"Failed to add output node to topology.");

	CHECK_HR(pSourceNode->ConnectOutput(0, pOutputNode, 0), 
		"Failed to connect the source node to the output node.");

	CHECK_HR(pSession->SetTopology(0, pTopology), 
		"Failed to set the topology on the session.");

	PropVariantInit(&varStart);

	CHECK_HR(pSession->Start(&GUID_NULL, &varStart), 
		"Failed to start session.\n");

done:

	printf("finished.\n");
	auto c = getchar();

	SAFE_RELEASE(pSourceResolver);
	SAFE_RELEASE(uSource);
	SAFE_RELEASE(pTopology);
	SAFE_RELEASE(pSourcePD);
	SAFE_RELEASE(pSourceSD);
	SAFE_RELEASE(pSource);
	SAFE_RELEASE(pSession);
	SAFE_RELEASE(pHandler);
	SAFE_RELEASE(pActivate);
	SAFE_RELEASE(pSourceNode);
  SAFE_RELEASE(pOutputNode);
	SAFE_RELEASE(pOutputSink);
	SAFE_RELEASE(pOutputNodeMediaType);

	return 0;
}

/**
* Initialises a new empty Window to host the video renderer and
* starts the message loop. This function needs to be called on a 
* separate thread as it does not return until the Window is closed.
*/
DWORD InitializeWindow(LPVOID lpThreadParameter)
{
	WNDCLASS wc = { 0 };

	wc.lpfnWndProc = DefWindowProc;
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

	return 0;
}

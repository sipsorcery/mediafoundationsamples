/******************************************************************************
* Filename: MFTopologyToSourceReader.cpp
*
* Description:
* This file contains a C++ console application that reads audio and video samples
* from an mp4 file using a topology (to handle required media format conversions)
* and a source reader to get the raw samples.
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* Status:
* Work in Progress.
*
* History:
* 22 Jan 2020	  Aaron Clauson	  Created, Dublin, Ireland.
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

// Add a source node to a topology.
HRESULT AddSourceNode(
	IMFTopology* pTopology,           // Topology.
	IMFMediaSource* pSource,          // Media source.
	IMFPresentationDescriptor* pPD,   // Presentation descriptor.
	IMFStreamDescriptor* pSD,         // Stream descriptor.
	IMFTopologyNode** ppNode)         // Receives the node pointer.
{
	IMFTopologyNode* pNode = NULL;

	CHECK_HR(MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pNode), "Failed to create topology node.");
	CHECK_HR(pNode->SetUnknown(MF_TOPONODE_SOURCE, pSource), "Failed to set source on topology node.");
	CHECK_HR(pNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD), "Failed to set presentation descriptor on topology node.");
	CHECK_HR(pNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSD), "Failed to set stream descriptor on topology node.");
	CHECK_HR(pTopology->AddNode(pNode), "Failed to add node to topology.");

	// Return the pointer to the caller.
	*ppNode = pNode;
	(*ppNode)->AddRef();

done:
	SAFE_RELEASE(&pNode);
	return S_OK;
}

// Add an output node to a topology.
HRESULT AddOutputNode(
	IMFTopology* pTopology,     // Topology.
	IMFActivate* pActivate,     // Media sink activation object.
	DWORD dwId,                 // Identifier of the stream sink.
	IMFTopologyNode** ppNode)   // Receives the node pointer.
{
	IMFTopologyNode* pNode = NULL;

	CHECK_HR(MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pNode), "Failed to create topology node.");
	CHECK_HR(pNode->SetObject(pActivate), "Failed to set sink on topology node.");
	CHECK_HR(pNode->SetUINT32(MF_TOPONODE_STREAMID, dwId), "Failed to set stream ID on topology node.");
	CHECK_HR(pNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE), "Failed to set no shutdown on topology node.");
	CHECK_HR(pTopology->AddNode(pNode), "Failed to add node to the topology.");

	// Return the pointer to the caller.
	*ppNode = pNode;
	(*ppNode)->AddRef();

done:
	SAFE_RELEASE(&pNode);
	return S_OK;
}

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
	IMFActivate *pAudioActivate = NULL, * pVideoActivate = NULL;
	PROPVARIANT varStart;
	DWORD sourceStreamCount = 0;
	IMFStreamSink *pOutputSink = NULL;
	IMFMediaType *pOutputNodeMediaType = NULL;
	IMFTopologyNode* pAudioSourceNode = NULL, * pVideoSourceNode = NULL;
	IMFTopologyNode* pAudioSinkNode = NULL, * pVideoSinkNode = NULL;

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

	// Iterate over the available source streams and create renderer's.
	for (DWORD i = 0; i < sourceStreamCount; i++)
	{
		BOOL fSelected = FALSE;
		GUID guidMajorType;

		CHECK_HR(pSourcePD->GetStreamDescriptorByIndex(i, &fSelected, &pSourceSD), 
			"Failed to get stream descriptor from presentation descriptor.");

		CHECK_HR(pSourceSD->GetMediaTypeHandler(&pHandler), 
			"Failed to create media type handler from presentation descriptor.");

		CHECK_HR(pHandler->GetMajorType(&guidMajorType), 
			"Failed to get media type handler from source stream.");

		if (guidMajorType == MFMediaType_Audio && fSelected)
		{
			printf("Creating audio renderer for stream index %i.\n", i);

			CHECK_HR(MFCreateAudioRendererActivate(&pAudioActivate), "Failed to create audio renderer activate object.");
			CHECK_HR(AddSourceNode(pTopology, pSource, pSourcePD, pSourceSD, &pAudioSourceNode), "Failed to add audio source node");
			CHECK_HR(AddOutputNode(pTopology, pAudioActivate, 0, &pAudioSinkNode), "Failed to add audio output node.");
			CHECK_HR(pAudioSourceNode->ConnectOutput(0, pAudioSinkNode, 0), "Failed to connect audio source and sink nodes.");
		}
		else if (guidMajorType == MFMediaType_Video && fSelected)
		{
			printf("Creating video renderer for stream index %i.\n", i);

			CHECK_HR(MFCreateVideoRendererActivate(_hwnd, &pVideoActivate), "Failed to create video renderer activate object.");
			CHECK_HR(AddSourceNode(pTopology, pSource, pSourcePD, pSourceSD, &pVideoSourceNode), "Failed to add video source node");
			CHECK_HR(AddOutputNode(pTopology, pVideoActivate, 0, &pVideoSinkNode), "Failed to add video output node.");
			CHECK_HR(pVideoSourceNode->ConnectOutput(0, pVideoSinkNode, 0), "Failed to connect video source and sink nodes.");
		}
		else
		{
			CHECK_HR(pSourcePD->DeselectStream(i), "Error deselecting source stream.");
		}
	}

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
	SAFE_RELEASE(pAudioActivate);
	SAFE_RELEASE(pVideoActivate);
	SAFE_RELEASE(pAudioSourceNode);
  SAFE_RELEASE(pVideoSourceNode);
	SAFE_RELEASE(pAudioSinkNode);
	SAFE_RELEASE(pVideoSinkNode);
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

/******************************************************************************
* Filename: MFVideoEVR.cpp
*
* Description:
* This file contains a C++ console application that is attempting to play the
* video stream from a sample MP4 file the Windows Media Foundation API. 
* Specifically it's attempting to use the Enhanced Video Renderer
* (https://msdn.microsoft.com/en-us/library/windows/desktop/ms694916%28v=vs.85%29.aspx)
* to playback the video.
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 01 Jan 2015	  Aaron Clauson	  Created, Hobart, Australia.
* 15 Sep 2015   Aaron Clauson		Trying with webcam instead of file but still not working.
* 05 Jan 2020   Aaron Clauson   Applied Stack Overflow answer from https://bit.ly/2sQoMuP, 
*                               now works for a file source.
*
* License: Public Domain (no warranty, use at own risk)
/******************************************************************************/

#include "..\Common\MFUtility.h"

#include <d3d9.h>
#include <Dxva2api.h>
#include <evr.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfobjects.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <windowsx.h>

#include <iostream>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "evr.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "Strmiids")
#pragma comment(lib, "wmcodecdspuuid.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "Dxva2.lib")

#define VIDEO_WIDTH  640
#define VIDEO_HEIGHT 480
#define MEDIA_FILE_PATH L"../MediaFiles/big_buck_bunny.mp4"

// Forward function definitions.
DWORD InitializeWindow(LPVOID lpThreadParameter);
HRESULT GetVideoSourceFromFile(LPWSTR path, IMFMediaSource** ppVideoSource, IMFSourceReader** ppVideoReader);

// Constants 
const WCHAR CLASS_NAME[] = L"MFVideoEVR Window Class";
const WCHAR WINDOW_NAME[] = L"MFVideoEVR";

// Globals.
HWND _hwnd;

int main()
{
  IMFMediaSource* pVideoSource = NULL;
  IMFSourceReader* pVideoReader = NULL;
  IMFMediaType* videoSourceOutputType = NULL, * pvideoSourceModType = NULL;
  IMFMediaType* pVideoSourceOutType = NULL, *pImfEvrSinkType = NULL;
  IMFMediaType* pHintMediaType = NULL;
  IMFMediaSink* pVideoSink = NULL;
  IMFStreamSink* pStreamSink = NULL;
  IMFMediaTypeHandler* pSinkMediaTypeHandler = NULL, * pSourceMediaTypeHandler = NULL;
  IMFPresentationDescriptor* pSourcePresentationDescriptor = NULL;
  IMFStreamDescriptor* pSourceStreamDescriptor = NULL;
  IMFVideoRenderer* pVideoRenderer = NULL;
  IMFVideoDisplayControl* pVideoDisplayControl = NULL;
  IMFGetService* pService = NULL;
  IMFActivate* pActive = NULL;
  IMFPresentationClock* pClock = NULL;
  IMFPresentationTimeSource* pTimeSource = NULL;
  IDirect3DDeviceManager9* pD3DManager = NULL;
  IMFVideoSampleAllocator* pVideoSampleAllocator = NULL;
  IMFSample* pD3DVideoSample = NULL;
  IMF2DBuffer* p2DBuffer = NULL;
  IMFMediaBuffer* pDstBuffer = NULL;
  RECT rc = { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT };
  BOOL fSelected = false;

  IMFMediaEventGenerator* pEventGenerator = NULL;
  IMFMediaEventGenerator* pstreamSinkEventGenerator = NULL;
  MediaEventHandler mediaEvtHandler;
  MediaEventHandler streamSinkMediaEvtHandler;

  CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
    "COM initialisation failed.");

  CHECK_HR(MFStartup(MF_VERSION),
    "Media Foundation initialisation failed.");

  // Create a separate Window and thread to host the Video player.
  CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)InitializeWindow, NULL, 0, NULL);
  Sleep(1000);
  if (_hwnd == nullptr)
  {
    printf("Failed to initialise video window.\n");
    goto done;
  }

  if (_hwnd == nullptr)
  {
    printf("Failed to initialise video window.\n");
    goto done;
  }

  // ----- Set up Video sink (Enhanced Video Renderer). -----

  CHECK_HR(MFCreateVideoRendererActivate(_hwnd, &pActive),
    "Failed to created video rendered activation context.");

  CHECK_HR(pActive->ActivateObject(IID_IMFMediaSink, (void**)&pVideoSink),
    "Failed to activate IMFMediaSink interface on video sink.");

  // Initialize the renderer before doing anything else including querying for other interfaces,
  // see https://msdn.microsoft.com/en-us/library/windows/desktop/ms704667(v=vs.85).aspx.
  CHECK_HR(pVideoSink->QueryInterface(__uuidof(IMFVideoRenderer), (void**)&pVideoRenderer),
    "Failed to get video Renderer interface from EVR media sink.");

  CHECK_HR(pVideoRenderer->InitializeRenderer(NULL, NULL),
    "Failed to initialise the video renderer.");

  CHECK_HR(pVideoSink->QueryInterface(__uuidof(IMFGetService), (void**)&pService),
    "Failed to get service interface from EVR media sink.");

  CHECK_HR(pService->GetService(MR_VIDEO_RENDER_SERVICE, __uuidof(IMFVideoDisplayControl), (void**)&pVideoDisplayControl),
    "Failed to get video display control interface from service interface.");

  CHECK_HR(pVideoDisplayControl->SetVideoWindow(_hwnd),
    "Failed to SetVideoWindow.");

  CHECK_HR(pVideoDisplayControl->SetVideoPosition(NULL, &rc),
    "Failed to SetVideoPosition.");

  CHECK_HR(pVideoSink->GetStreamSinkByIndex(0, &pStreamSink),
    "Failed to get video renderer stream by index.");

  CHECK_HR(pStreamSink->GetMediaTypeHandler(&pSinkMediaTypeHandler),
    "Failed to get media type handler for stream sink.");

  DWORD sinkMediaTypeCount = 0;
  CHECK_HR(pSinkMediaTypeHandler->GetMediaTypeCount(&sinkMediaTypeCount),
    "Failed to get sink media type count.");

  std::cout << "Sink media type count: " << sinkMediaTypeCount << "." << std::endl;

  // ----- Set up Video source. -----

  CHECK_HR(GetVideoSourceFromFile(MEDIA_FILE_PATH, &pVideoSource, &pVideoReader),
    "Failed to get file video source.");

  CHECK_HR(pVideoReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, false),
    "Failed to deselect all streams on video reader.");

  CHECK_HR(pVideoReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &videoSourceOutputType),
    "Error retrieving current media type from first video stream.");

  CHECK_HR(pVideoReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE),
    "Failed to set the first video stream on the source reader.");

  CHECK_HR(pVideoSource->CreatePresentationDescriptor(&pSourcePresentationDescriptor),
    "Failed to create the presentation descriptor from the media source.");

  CHECK_HR(pSourcePresentationDescriptor->GetStreamDescriptorByIndex(0, &fSelected, &pSourceStreamDescriptor),
    "Failed to get source stream descriptor from presentation descriptor.");

  CHECK_HR(pSourceStreamDescriptor->GetMediaTypeHandler(&pSourceMediaTypeHandler),
    "Failed to get source media type handler.");

  DWORD srcMediaTypeCount = 0;
  CHECK_HR(pSourceMediaTypeHandler->GetMediaTypeCount(&srcMediaTypeCount),
    "Failed to get source media type count.");

  std::cout << "Source media type count: " << srcMediaTypeCount << ", is first stream selected " << fSelected << "." << std::endl;
  std::cout << "Default output media type for source reader:" << std::endl;
  std::cout << GetMediaTypeDescription(videoSourceOutputType) << std::endl << std::endl;

  // ----- Create a compatible media type and set on the source and sink. -----

    // Set the video output type on the file source.
  CHECK_HR(MFCreateMediaType(&pVideoSourceOutType), "Failed to create video output media type.");
  CHECK_HR(pVideoSourceOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set video output media major type.");
  CHECK_HR(pVideoSourceOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32), "Failed to set video sub-type attribute on media type.");
  CHECK_HR(pVideoSourceOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive), "Failed to set interlace mode attribute on media type.");
  CHECK_HR(pVideoSourceOutType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE), "Failed to set independent samples attribute on media type.");
  CHECK_HR(MFSetAttributeRatio(pVideoSourceOutType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set pixel aspect ratio attribute on media type.");
  CHECK_HR(CopyAttribute(videoSourceOutputType, pVideoSourceOutType, MF_MT_FRAME_SIZE), "Failed to copy video frame size attribute to media type.");
  CHECK_HR(CopyAttribute(videoSourceOutputType, pVideoSourceOutType, MF_MT_FRAME_RATE), "Failed to copy video frame rate attribute to media type.");

  // Set the video input type on the EVR sink.
  CHECK_HR(MFCreateMediaType(&pImfEvrSinkType), "Failed to create video output media type.");
  CHECK_HR(pImfEvrSinkType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set video output media major type.");
  CHECK_HR(pImfEvrSinkType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32), "Failed to set video sub-type attribute on media type.");
  CHECK_HR(pImfEvrSinkType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive), "Failed to set interlace mode attribute on media type.");
  CHECK_HR(pImfEvrSinkType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE), "Failed to set independent samples attribute on media type.");
  CHECK_HR(MFSetAttributeRatio(pImfEvrSinkType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set pixel aspect ratio attribute on media type.");
  CHECK_HR(CopyAttribute(videoSourceOutputType, pImfEvrSinkType, MF_MT_FRAME_SIZE), "Failed to copy video frame size attribute to media type.");
  CHECK_HR(CopyAttribute(videoSourceOutputType, pImfEvrSinkType, MF_MT_FRAME_RATE), "Failed to copy video frame rate attribute to media type.");
  CHECK_HR(pSinkMediaTypeHandler->SetCurrentMediaType(pImfEvrSinkType),
    "Failed to set input media type on EVR sink.");

  CHECK_HR(pVideoReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pVideoSourceOutType),
    "Failed to set output media type on source reader.");

  std::cout << "EVR input media type defined as:" << std::endl;
  std::cout << GetMediaTypeDescription(pImfEvrSinkType) << std::endl << std::endl;

  // ----- Set up event handler for sink events otherwise memory leaks. -----

  CHECK_HR(pVideoSink->QueryInterface(IID_IMFMediaEventGenerator, (void**)&pEventGenerator),
    "Video sink doesn't support IMFMediaEventGenerator interface.");

  CHECK_HR(pEventGenerator->BeginGetEvent((IMFAsyncCallback*)&mediaEvtHandler, pEventGenerator),
    "BeginGetEvent on media generator failed.");

  CHECK_HR(pStreamSink->QueryInterface(IID_IMFMediaEventGenerator, (void**)&pstreamSinkEventGenerator),
    "Stream sink doesn't support IMFMediaEventGenerator interface.");

  CHECK_HR(pstreamSinkEventGenerator->BeginGetEvent((IMFAsyncCallback*)&streamSinkMediaEvtHandler, pstreamSinkEventGenerator),
    "BeginGetEvent on stream sink media generator failed.");

  // ----- Source and sink now configured. Set up remaining infrastructure and then start sampling. -----

  // Get Direct3D surface organised.
  // https://msdn.microsoft.com/fr-fr/library/windows/desktop/aa473823(v=vs.85).aspx
  CHECK_HR(MFGetService(pStreamSink, MR_VIDEO_ACCELERATION_SERVICE, IID_PPV_ARGS(&pVideoSampleAllocator)), "Failed to get IMFVideoSampleAllocator.");
  CHECK_HR(MFGetService(pVideoSink, MR_VIDEO_ACCELERATION_SERVICE, IID_PPV_ARGS(&pD3DManager)), "Failed to get Direct3D manager from EVR media sink.");
  CHECK_HR(pVideoSampleAllocator->SetDirectXManager(pD3DManager), "Failed to set D3DManager on video sample allocator.");
  CHECK_HR(pVideoSampleAllocator->InitializeSampleAllocator(1, pImfEvrSinkType), "Failed to initialise video sample allocator.");
  CHECK_HR(pVideoSampleAllocator->AllocateSample(&pD3DVideoSample), "Failed to allocate video sample.");
  CHECK_HR(pD3DVideoSample->GetBufferByIndex(0, &pDstBuffer), "Failed to get destination buffer.");
  CHECK_HR(pDstBuffer->QueryInterface(IID_PPV_ARGS(&p2DBuffer)), "Failed to get pointer to 2D buffer.");

  // Get clocks organised.
  CHECK_HR(MFCreatePresentationClock(&pClock), "Failed to create presentation clock.");
  CHECK_HR(MFCreateSystemTimeSource(&pTimeSource), "Failed to create system time source.");
  CHECK_HR(pClock->SetTimeSource(pTimeSource), "Failed to set time source.");
  CHECK_HR(pVideoSink->SetPresentationClock(pClock), "Failed to set presentation clock on video sink.");
  CHECK_HR(pClock->Start(0), "Error starting presentation clock.");

  // Start the sample read-write loop.
  IMFSample* videoSample = NULL;
  IMFMediaBuffer* pSrcBuffer = NULL;
  BYTE* pbBuffer = NULL;
  DWORD streamIndex, flags;
  LONGLONG llTimeStamp;
  UINT32 uiAttribute = 0;
  DWORD dwBuffer = 0;

  while (true)
  {
    CHECK_HR(pVideoReader->ReadSample(
      MF_SOURCE_READER_FIRST_VIDEO_STREAM,
      0,                              // Flags.
      &streamIndex,                   // Receives the actual stream index. 
      &flags,                         // Receives status flags.
      &llTimeStamp,                   // Receives the time stamp.
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
      UINT sampleCount = 0;
      LONGLONG sampleDuration = 0;
      CHECK_HR(videoSample->GetCount(&sampleCount), "Failed to get video sample count.");
      CHECK_HR(videoSample->GetSampleDuration(&sampleDuration), "Failed to get video sample duration.");

      //printf("Attempting to write sample to stream sink, sample count %d, sample duration %llu, sample time %llu.\n", sampleCount, sampleDuration, llTimeStamp);

      CHECK_HR(pD3DVideoSample->SetSampleTime(llTimeStamp), "Failed to set D3D video sample time.");
      CHECK_HR(pD3DVideoSample->SetSampleDuration(sampleDuration), "Failed to set D3D video sample duration.");
      CHECK_HR(videoSample->ConvertToContiguousBuffer(&pSrcBuffer), "Failed to get buffer from video sample.");
      CHECK_HR(pSrcBuffer->Lock(&pbBuffer, NULL, &dwBuffer), "Failed to lock sample buffer.");
      CHECK_HR(p2DBuffer->ContiguousCopyFrom(pbBuffer, dwBuffer), "Failed to unlock sample buffer.");
      CHECK_HR(pSrcBuffer->Unlock(), "Failed to unlock source buffer.\n");

      CHECK_HR(videoSample->GetUINT32(MFSampleExtension_FrameCorruption, &uiAttribute), "Failed to get frame corruption attribute.");
      CHECK_HR(pD3DVideoSample->SetUINT32(MFSampleExtension_FrameCorruption, uiAttribute), "Failed to set frame corruption attribute.");
      CHECK_HR(videoSample->GetUINT32(MFSampleExtension_Discontinuity, &uiAttribute), "Failed to get discontinuity attribute.");
      CHECK_HR(pD3DVideoSample->SetUINT32(MFSampleExtension_Discontinuity, uiAttribute), "Failed to set discontinuity attribute.");
      CHECK_HR(videoSample->GetUINT32(MFSampleExtension_CleanPoint, &uiAttribute), "Failed to get clean point attribute.");
      CHECK_HR(pD3DVideoSample->SetUINT32(MFSampleExtension_CleanPoint, uiAttribute), "Failed to set clean point attribute.");

      CHECK_HR(pStreamSink->ProcessSample(pD3DVideoSample), "Stream sink process sample failed.");

      Sleep(sampleDuration / 10000); // Duration is given in 100's of nano seconds.
    }

    SAFE_RELEASE(pSrcBuffer);
    SAFE_RELEASE(videoSample);
  }

done:

  printf("finished.\n");
  auto c = getchar();

  SAFE_RELEASE(p2DBuffer);
  SAFE_RELEASE(pDstBuffer);
  SAFE_RELEASE(pVideoReader);
  SAFE_RELEASE(videoSourceOutputType);
  SAFE_RELEASE(pvideoSourceModType);
  SAFE_RELEASE(pImfEvrSinkType);
  SAFE_RELEASE(pHintMediaType);
  SAFE_RELEASE(pVideoSink);
  SAFE_RELEASE(pStreamSink);
  SAFE_RELEASE(pSinkMediaTypeHandler);
  SAFE_RELEASE(pSourceMediaTypeHandler);
  SAFE_RELEASE(pSourcePresentationDescriptor);
  SAFE_RELEASE(pSourceStreamDescriptor);
  SAFE_RELEASE(pVideoRenderer);
  SAFE_RELEASE(pVideoDisplayControl);
  SAFE_RELEASE(pService);
  SAFE_RELEASE(pActive);
  SAFE_RELEASE(pClock);
  SAFE_RELEASE(pTimeSource);
  SAFE_RELEASE(pD3DManager);
  SAFE_RELEASE(pVideoSampleAllocator);
  SAFE_RELEASE(pD3DVideoSample);
  SAFE_RELEASE(pEventGenerator);
  SAFE_RELEASE(pstreamSinkEventGenerator);

  return 0;
}

/**
* Gets a video source reader from a media file.
* @param[in] path: the media file path to get the source reader for.
* @param[out] ppVideoSource: will be set with the source for the reader if successful.
* @param[out] ppVideoReader: will be set with the reader if successful.
* @@Returns S_OK if successful or an error code if not.
*/
HRESULT GetVideoSourceFromFile(LPWSTR path, IMFMediaSource** ppVideoSource, IMFSourceReader** ppVideoReader)
{
  IMFSourceResolver* pSourceResolver = NULL;
  IUnknown* uSource = NULL;

  IMFAttributes* pVideoReaderAttributes = NULL;
  MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;

  HRESULT hr = S_OK;

  hr = MFCreateSourceResolver(&pSourceResolver);
  CHECK_HR(hr, "MFCreateSourceResolver failed.");

  hr = pSourceResolver->CreateObjectFromURL(
    path,                       // URL of the source.
    MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
    NULL,                       // Optional property store.
    &ObjectType,                // Receives the created object type. 
    &uSource                    // Receives a pointer to the media source. 
  );
  CHECK_HR(hr, "Failed to create media source resolver for file.");

  hr = uSource->QueryInterface(IID_PPV_ARGS(ppVideoSource));
  CHECK_HR(hr, "Failed to create media file source.");

  hr = MFCreateAttributes(&pVideoReaderAttributes, 1);
  CHECK_HR(hr, "Failed to create attributes object for video reader.");

  hr = pVideoReaderAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, 1);
  CHECK_HR(hr, "Failed to set enable video processing attribute type for reader config.");

  hr = MFCreateSourceReaderFromMediaSource(*ppVideoSource, pVideoReaderAttributes, ppVideoReader);
  CHECK_HR(hr, "Error creating media source reader.");

done:

  SAFE_RELEASE(pSourceResolver);
  SAFE_RELEASE(uSource);
  SAFE_RELEASE(pVideoReaderAttributes);

  return hr;
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
      VIDEO_WIDTH,
      VIDEO_HEIGHT,
      NULL,
      NULL,
      GetModuleHandle(NULL),
      NULL
    );

    if (_hwnd)
    {
      ShowWindow(_hwnd, SW_SHOWDEFAULT);
      MSG msg = { 0 };

      while (true)
      {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
        else
        {
          Sleep(1);
        }
      }
    }
  }

  return 0;
}

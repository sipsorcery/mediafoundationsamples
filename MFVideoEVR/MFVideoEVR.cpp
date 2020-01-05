/******************************************************************************
* Filename: MFVideoEVR.cpp
*
* Description:
* This file contains a C++ console application that is attempting to play the
* video stream from a sample MP4 file or webcam using the Windows Media Foundation 
* API. Specifically it's attempting to use the Enhanced Video Renderer 
* (https://msdn.microsoft.com/en-us/library/windows/desktop/ms694916%28v=vs.85%29.aspx)
* to playback the video.
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 01 Jan 2015	  Aaron Clauson	  Created, Hobart, Australia.
* 15 Sep 2015   Aaron Clauson		Trying with webcam instead of file but still no idea 
* 05 Jan 2020   Aaron Clauson   Applied Stack Overflow answer from https://bit.ly/2sQoMuP.
*                               and finally for some video to display.
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

// Constants 
const WCHAR CLASS_NAME[] = L"MFVideoEVR Window Class";
const WCHAR WINDOW_NAME[] = L"MFVideoEVR";

// Globals.
HWND _hwnd;

int main()
{
  IMFSourceReader* videoReader = NULL;
  IMFMediaType* videoSourceOutputType = NULL, * pvideoSourceModType = NULL;
  IMFSourceResolver* pSourceResolver = NULL;
  IUnknown* uSource = NULL;
  IMFMediaSource* mediaFileSource = NULL;
  IMFAttributes* pVideoReaderAttributes = NULL;
  IMFMediaType* pVideoOutType = NULL;
  IMFMediaSink* pVideoSink = NULL;
  IMFStreamSink* pStreamSink = NULL;
  IMFMediaTypeHandler* pMediaTypeHandler = NULL;
  IMFVideoRenderer* pVideoRenderer = NULL;
  IMFVideoDisplayControl* pVideoDisplayControl = NULL;
  IMFGetService* pService = NULL;
  IMFActivate* pActive = NULL;
  IMFPresentationClock* pClock = NULL;
  IMFPresentationTimeSource* pTimeSource = NULL;
  IDirect3DDeviceManager9* pD3DManager = NULL;
  IMFVideoSampleAllocator* pVideoSampleAllocator = NULL;
  IMFSample* pD3DVideoSample = NULL;
  MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;
  RECT rc = { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT };

  CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
    "COM initialisation failed.");

  CHECK_HR(MFStartup(MF_VERSION),
    "Media Foundation initialisation failed.");

  CHECK_HR(MFTRegisterLocalByCLSID(
    __uuidof(CColorConvertDMO),
    MFT_CATEGORY_VIDEO_PROCESSOR,
    L"",
    MFT_ENUM_FLAG_SYNCMFT,
    0,
    NULL,
    0,
    NULL), 
    "Error registering colour converter DSP.\n");

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

  // Configure Enhanced Video Renderer sink and assign it to the Window we just created..
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

  // Set up the reader for the file.
  CHECK_HR(MFCreateSourceResolver(&pSourceResolver), 
    "MFCreateSourceResolver failed.");

  CHECK_HR(pSourceResolver->CreateObjectFromURL(
    MEDIA_FILE_PATH,            // URL of the source.
    MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
    NULL,                       // Optional property store.
    &ObjectType,                // Receives the created object type. 
    &uSource),                  // Receives a pointer to the media source. 
    "Failed to create media source resolver for file.");

  CHECK_HR(uSource->QueryInterface(IID_PPV_ARGS(&mediaFileSource)), 
    "Failed to create media file source.");

  CHECK_HR(MFCreateAttributes(&pVideoReaderAttributes, 1),
    "Failed to create attributes object for video reader.");
  
  CHECK_HR(pVideoReaderAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, 1),
    "Failed to set enable video processing attribute type for reader config.");

  CHECK_HR(MFCreateSourceReaderFromMediaSource(mediaFileSource, pVideoReaderAttributes, &videoReader),
    "Error creating media source reader.");

  CHECK_HR(videoReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &videoSourceOutputType),
    "Error retrieving current media type from first video stream.");

  std::cout << "Default output media type for source reader:" << std::endl;
  std::cout << GetMediaTypeDescription(videoSourceOutputType) << std::endl << std::endl;

  // Set the video output type on the source reader.
  CHECK_HR(MFCreateMediaType(&pvideoSourceModType), "Failed to create video output media type.\n");
  CHECK_HR(pvideoSourceModType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set video output media major type.\n");
  CHECK_HR(pvideoSourceModType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32), "Failed to set video sub-type attribute on EVR input media type.\n");
  CHECK_HR(pvideoSourceModType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive), "Failed to set interlace mode attribute on EVR input media type.\n");
  CHECK_HR(pvideoSourceModType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE), "Failed to set independent samples attribute on EVR input media type.\n");
  CHECK_HR(MFSetAttributeRatio(pvideoSourceModType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set pixel aspect ratio attribute on EVR input media type.\n");
  CHECK_HR(CopyAttribute(videoSourceOutputType, pvideoSourceModType, MF_MT_FRAME_SIZE), "Failed to copy video frame size attribute from input file to output sink.\n");
  CHECK_HR(CopyAttribute(videoSourceOutputType, pvideoSourceModType, MF_MT_FRAME_RATE), "Failed to copy video frame rate attribute from input file to output sink.\n");

  CHECK_HR(videoReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pvideoSourceModType), 
    "Failed to set output media type on source reader.");

  std::cout << "Output media type set on source reader:" << std::endl;
  std::cout << GetMediaTypeDescription(pvideoSourceModType) << std::endl << std::endl;

  CHECK_HR(pVideoSink->GetStreamSinkByIndex(0, &pStreamSink),
    "Failed to get video renderer stream by index.");

  CHECK_HR(pStreamSink->GetMediaTypeHandler(&pMediaTypeHandler), 
    "Failed to get media type handler.");

  // Set the video input type on the EVR sink.
  CHECK_HR(MFCreateMediaType(&pVideoOutType), "Failed to create video output media type.\n");
  CHECK_HR(pVideoOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set video output media major type.\n");
  CHECK_HR(pVideoOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32), "Failed to set video sub-type attribute on EVR input media type.\n");
  CHECK_HR(pVideoOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive), "Failed to set interlace mode attribute on EVR input media type.\n");
  CHECK_HR(pVideoOutType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE), "Failed to set independent samples attribute on EVR input media type.\n");
  CHECK_HR(MFSetAttributeRatio(pVideoOutType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set pixel aspect ratio attribute on EVR input media type.\n");
  CHECK_HR(CopyAttribute(videoSourceOutputType, pVideoOutType, MF_MT_FRAME_SIZE), "Failed to copy video frame size attribute from input file to output sink.\n");
  CHECK_HR(CopyAttribute(videoSourceOutputType, pVideoOutType, MF_MT_FRAME_RATE), "Failed to copy video frame rate attribute from input file to output sink.\n");

  CHECK_HR(pMediaTypeHandler->SetCurrentMediaType(pVideoOutType), 
    "Failed to set input media type on EVR sink.");

  std::cout << "Input media type set on EVR:" << std::endl;
  std::cout << GetMediaTypeDescription(pVideoOutType) << std::endl << std::endl;

  // Get Direct3D surface organised.
  // https://msdn.microsoft.com/fr-fr/library/windows/desktop/aa473823(v=vs.85).aspx
  CHECK_HR(MFGetService(pStreamSink, MR_VIDEO_ACCELERATION_SERVICE, IID_PPV_ARGS(&pVideoSampleAllocator)), "Failed to get IMFVideoSampleAllocator.");
  CHECK_HR(MFGetService(pVideoSink, MR_VIDEO_ACCELERATION_SERVICE, IID_PPV_ARGS(&pD3DManager)), "Failed to get Direct3D manager from EVR media sink.");
  CHECK_HR(pVideoSampleAllocator->SetDirectXManager(pD3DManager), "Failed to set D3DManager on video sample allocator.");
  CHECK_HR(pVideoSampleAllocator->InitializeSampleAllocator(1, pVideoOutType), "Failed to initialise video sample allocator.");
  CHECK_HR(pVideoSampleAllocator->AllocateSample(&pD3DVideoSample), "Failed to allocate video sample.");

  // Get clocks organised.
  CHECK_HR(MFCreatePresentationClock(&pClock), "Failed to create presentation clock.");
  CHECK_HR(MFCreateSystemTimeSource(&pTimeSource), "Failed to create system time source.");
  CHECK_HR(pClock->SetTimeSource(pTimeSource), "Failed to set time source.");
  CHECK_HR(pVideoSink->SetPresentationClock(pClock), "Failed to set presentation clock on video sink.");
  CHECK_HR(pClock->Start(0), "Error starting presentation clock.");

  // Start the sample read-write loop.
  IMFSample* videoSample = NULL;
  IMFMediaBuffer* pSrcBuffer = NULL;
  IMFMediaBuffer* pDstBuffer = NULL;
  IMF2DBuffer* p2DBuffer = NULL;
  BYTE* pbBuffer = NULL;
  DWORD streamIndex, flags;
  LONGLONG llTimeStamp;
  UINT32 uiAttribute = 0;
  DWORD dwBuffer = 0;

  while (true)
  {
    CHECK_HR(videoReader->ReadSample(
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
      CHECK_HR(pD3DVideoSample->GetBufferByIndex(0, &pDstBuffer), "Failed to get destination buffer.");
      CHECK_HR(pDstBuffer->QueryInterface(IID_PPV_ARGS(&p2DBuffer)), "Failed to get pointer to 2D buffer.");
      CHECK_HR(p2DBuffer->ContiguousCopyFrom(pbBuffer, dwBuffer), "Failed to unlock sample buffer.");
      CHECK_HR(pSrcBuffer->Unlock(), ".\n");

      CHECK_HR(videoSample->GetUINT32(MFSampleExtension_FrameCorruption, &uiAttribute), "Failed to get frame corruption attribute.");
      CHECK_HR(pD3DVideoSample->SetUINT32(MFSampleExtension_FrameCorruption, uiAttribute), "Failed to set frame corruption attribute.");
      CHECK_HR(videoSample->GetUINT32(MFSampleExtension_Discontinuity, &uiAttribute), "Failed to get discontinuity attribute.");
      CHECK_HR(pD3DVideoSample->SetUINT32(MFSampleExtension_Discontinuity, uiAttribute), "Failed to set discontinuity attribute.");
      CHECK_HR(videoSample->GetUINT32(MFSampleExtension_CleanPoint, &uiAttribute), "Failed to get clean point attribute.");
      CHECK_HR(pD3DVideoSample->SetUINT32(MFSampleExtension_CleanPoint, uiAttribute), "Failed to set clean point attribute.");

      CHECK_HR(pStreamSink->ProcessSample(pD3DVideoSample), "Streamsink process sample failed.");

      Sleep(sampleDuration / 10000); // Duration is given in 10's of nano seconds.
    }

    SAFE_RELEASE(p2DBuffer);
    SAFE_RELEASE(pDstBuffer);
    SAFE_RELEASE(pSrcBuffer);
    SAFE_RELEASE(videoSample);
  }

done:

  printf("finished.\n");
  auto c = getchar();

  SAFE_RELEASE(videoReader);
  SAFE_RELEASE(videoSourceOutputType);
  SAFE_RELEASE(pvideoSourceModType);
  SAFE_RELEASE(pSourceResolver);
  SAFE_RELEASE(uSource);
  SAFE_RELEASE(mediaFileSource);
  SAFE_RELEASE(pVideoReaderAttributes);
  SAFE_RELEASE(pVideoOutType);
  SAFE_RELEASE(pVideoSink);
  SAFE_RELEASE(pStreamSink);
  SAFE_RELEASE(pMediaTypeHandler);
  SAFE_RELEASE(pVideoRenderer);
  SAFE_RELEASE(pVideoDisplayControl);
  SAFE_RELEASE(pService);
  SAFE_RELEASE(pActive);
  SAFE_RELEASE(pClock);
  SAFE_RELEASE(pTimeSource);
  SAFE_RELEASE(pD3DManager);
  SAFE_RELEASE(pVideoSampleAllocator);
  SAFE_RELEASE(pD3DVideoSample);

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

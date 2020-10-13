/******************************************************************************
* Filename: MFVideoEVRWebcam.cpp
*
* Description:
* This file contains a C++ console application that is attempting to play the
* video stream from a webcam source using the Windows Media Foundation
* API. Specifically it's attempting to use the Enhanced Video Renderer
* (https://msdn.microsoft.com/en-us/library/windows/desktop/ms694916%28v=vs.85%29.aspx)
* to playback the video.

* The difference between this sample and the MFVideoEVR sample
* is that the webcam and the EVR can potentially have different pixel formats and
* require a colour conversion transform between the source and sink.
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 01 Jan 2015	  Aaron Clauson	  Created, Hobart, Australia.
* 15 Sep 2015   Aaron Clauson		Trying with webcam instead of file but still not working.
* 05 Jan 2020   Aaron Clauson   Applied Stack Overflow answer from https://bit.ly/2sQoMuP,
*                               now works for a file source.
* 07 Jan 2020   Aaron Clauson   Split from MFVideoEVR sample. File and webcam sources
*                               require different treatment, large enough to warrant new sample.
* 10 Jan 2020   Aaron Clauson   Removed colour conversion MFT after discovering setting the
*                               MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING on the reader does the same thing.
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
#define VIDEO_FRAME_RATE 5
#define WEBCAM_PIXEL_FORMAT MFVideoFormat_RGB24
//#define WEBCAM_PIXEL_FORMAT MFVideoFormat_YUY2
#define RENDERER_PIXEL_FORMAT MFVideoFormat_RGB32
//#define PIXEL_FORMAT MFVideoFormat_RGB32
//#define PIXEL_FORMAT MFVideoFormat_YUY2
//#define PIXEL_FORMAT MFVideoFormat_I420
#define WEBCAM_DEVICE_INDEX 0	  // Adjust according to desired video capture device.

// Forward function definitions.
DWORD InitializeWindow(LPVOID lpThreadParameter);

// Constants 
const WCHAR CLASS_NAME[] = L"MFVideoEVRWebcam Window Class";
const WCHAR WINDOW_NAME[] = L"MFVideoEVRWebcam";

// Globals.
HWND _hwnd;

int main()
{
  IMFMediaSource* pVideoSource = NULL;
  IMFSourceReader* pVideoReader = NULL;
  IMFMediaType* pVideoSourceOutputType = NULL, * pWebCamMatchingType = NULL, * pWebcamSourceType = NULL, *pSourceReaderType = NULL;
  IMFMediaType* pImfEvrSinkType = NULL;
  IMFMediaType* pHintMediaType = NULL;
  IMFMediaSink* pVideoSink = NULL;
  IMFStreamSink* pStreamSink = NULL;
  IMFSinkWriter* pSinkWriter = NULL;
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
  RECT rc = { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT };
  BOOL fSelected = false;

  CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
    "COM initialisation failed.");

  CHECK_HR(MFStartup(MF_VERSION),
    "Media Foundation initialisation failed.");

  //CHECK_HR(ListCaptureDevices(DeviceType::Video),
  //  "Error listing video capture devices.");

  CHECK_HR(ListVideoDevicesWithBriefFormat(), "Error listing video capture devices.");

  // No longer needed due to setting MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING on source reader.
  // Need the color converter DSP for conversions between YUV, RGB etc.
  //CHECK_HR(MFTRegisterLocalByCLSID(
  //  __uuidof(CColorConvertDMO),
  //  MFT_CATEGORY_VIDEO_PROCESSOR,
  //  L"",
  //  MFT_ENUM_FLAG_SYNCMFT,
  //  0,
  //  NULL,
  //  0,
  //  NULL),
  //  "Error registering colour converter DSP.");

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

  // ----- Set up webcam video source. -----

  CHECK_HR(GetVideoSourceFromDevice(WEBCAM_DEVICE_INDEX, &pVideoSource, &pVideoReader),
    "Failed to get webcam video source.");

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

  // ----- Attempt to set the desired media type on the webcam source. -----

  CHECK_HR(MFCreateMediaType(&pWebcamSourceType), "Failed to create webcam output media type.");

  CHECK_HR(FindMatchingVideoType(pSourceMediaTypeHandler, WEBCAM_PIXEL_FORMAT, VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FRAME_RATE, pWebcamSourceType),
    "No matching webcam media type was found.");

  // This check is not necessary if the media type was from the list of supported types.
  // It is useful if the media type is constructed manually. It is left here for demonstration purposes. 
  CHECK_HR(pSourceMediaTypeHandler->IsMediaTypeSupported(pWebcamSourceType, &pWebCamMatchingType), "Webcam does not support requested options.");

  if (pWebCamMatchingType != NULL) {
    // If IsMediaTypeSupported supplied us with the closest matching media type use that.
    CHECK_HR(pSourceMediaTypeHandler->SetCurrentMediaType(pWebCamMatchingType), "Failed to set media type on source.");
  }
  else {
    // If IsMediaTypeSupported did not supply us a new type the typ checked must have been good enough use that.
    CHECK_HR(pSourceMediaTypeHandler->SetCurrentMediaType(pWebcamSourceType), "Failed to set media type on source.");
  }

  CHECK_HR(pSourceMediaTypeHandler->GetCurrentMediaType(&pVideoSourceOutputType),
    "Error retrieving current media type from first video stream.");

  std::cout << "Webcam media type:" << std::endl;
  std::cout << GetMediaTypeDescription(pVideoSourceOutputType) << std::endl << std::endl;

  // ----- Set the video input type on the EVR sink.

  CHECK_HR(MFCreateMediaType(&pImfEvrSinkType), "Failed to create video output media type.");
  CHECK_HR(pImfEvrSinkType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set video output media major type.");
  CHECK_HR(pImfEvrSinkType->SetGUID(MF_MT_SUBTYPE, RENDERER_PIXEL_FORMAT), "Failed to set video sub-type attribute on media type.");
  CHECK_HR(pImfEvrSinkType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive), "Failed to set interlace mode attribute on media type.");
  CHECK_HR(pImfEvrSinkType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE), "Failed to set independent samples attribute on media type.");
  CHECK_HR(MFSetAttributeRatio(pImfEvrSinkType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set pixel aspect ratio attribute on media type.");
  CHECK_HR(MFSetAttributeSize(pImfEvrSinkType, MF_MT_FRAME_SIZE, VIDEO_WIDTH, VIDEO_HEIGHT), "Failed to set the frame size attribute on media type.");
  CHECK_HR(MFSetAttributeSize(pImfEvrSinkType, MF_MT_FRAME_RATE, VIDEO_FRAME_RATE, 1), "Failed to set the frame rate attribute on media type.");

  CHECK_HR(pSinkMediaTypeHandler->SetCurrentMediaType(pImfEvrSinkType),
   "Failed to set input media type on EVR sink.");

  std::cout << "EVR input media:" << std::endl;
  std::cout << GetMediaTypeDescription(pImfEvrSinkType) << std::endl << std::endl;

  CHECK_HR(MFCreateMediaType(&pSourceReaderType), "Failed to source reader media type.");
  CHECK_HR(pImfEvrSinkType->CopyAllItems(pSourceReaderType), "Error copying media type attributes from EVR input to source reader media type.");

  // VERY IMPORTANT: Set the media type on the source reader to match the media type on the EVR. The
  // reader will do it's best to translate between the media type set on the webcam and the input type to the EVR.
  // If an error occurs copying the sample in the read-loop then it's usually because the reader could not translate
  // the types.
  CHECK_HR(pVideoReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pSourceReaderType),
    "Failed to set output media type on webcam source reader.");

  // ----- Source and sink now configured. Set up remaining infrastructure and then start sampling. -----

  // Get Direct3D surface organised.
  // https://msdn.microsoft.com/fr-fr/library/windows/desktop/aa473823(v=vs.85).aspx
  CHECK_HR(MFGetService(pStreamSink, MR_VIDEO_ACCELERATION_SERVICE, IID_PPV_ARGS(&pVideoSampleAllocator)), "Failed to get IMFVideoSampleAllocator.");
  CHECK_HR(MFGetService(pVideoSink, MR_VIDEO_ACCELERATION_SERVICE, IID_PPV_ARGS(&pD3DManager)), "Failed to get Direct3D manager from EVR media sink.");
  CHECK_HR(pVideoSampleAllocator->SetDirectXManager(pD3DManager), "Failed to set D3DManager on video sample allocator.");
  CHECK_HR(pVideoSampleAllocator->InitializeSampleAllocator(1, pImfEvrSinkType), "Failed to initialise video sample allocator.");
  CHECK_HR(pVideoSampleAllocator->AllocateSample(&pD3DVideoSample), "Failed to allocate video sample.");

  // Get clocks organised.
  CHECK_HR(MFCreatePresentationClock(&pClock), "Failed to create presentation clock.");
  CHECK_HR(MFCreateSystemTimeSource(&pTimeSource), "Failed to create system time source.");
  CHECK_HR(pClock->SetTimeSource(pTimeSource), "Failed to set time source.");
  CHECK_HR(pVideoSink->SetPresentationClock(pClock), "Failed to set presentation clock on video sink.");
  CHECK_HR(pClock->Start(0), "Error starting presentation clock.");

  // Start the sample read-write loop.
  IMFSample* pVideoSample = NULL;
  IMFMediaBuffer* pDstBuffer = NULL;
  IMF2DBuffer* p2DBuffer = NULL;
  BYTE* pbBuffer = NULL;
  DWORD streamIndex, flags;
  LONGLONG llTimeStamp;
  UINT32 uiAttribute = 0;
  DWORD dwBuffer = 0;
  DWORD d2dBufferLen = 0;

  LONGLONG evrTimestamp = 0;

  while (true)
  {
    CHECK_HR(pVideoReader->ReadSample(
      MF_SOURCE_READER_FIRST_VIDEO_STREAM,
      0,                              // Flags.
      &streamIndex,                   // Receives the actual stream index. 
      &flags,                         // Receives status flags.
      &llTimeStamp,                   // Receives the time stamp.
      &pVideoSample                    // Receives the sample or NULL.
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

    if (!pVideoSample)
    {
      printf("Null video sample.\n");
    }
    else
    {
      LONGLONG sampleDuration = 0;
      DWORD mftOutFlags;

      // ----- Video source sample. -----

      CHECK_HR(pVideoSample->SetSampleTime(llTimeStamp), "Error setting the video sample time.");
      CHECK_HR(pVideoSample->GetSampleDuration(&sampleDuration), "Failed to get video sample duration.");

      printf("Attempting to convert sample, sample duration %llu, sample time %llu, evr timestamp %llu.\n", sampleDuration, llTimeStamp, evrTimestamp);

      // Relies on the webcam pixel format being RGB24.
      //CreateBitmapFromSample(L"capture_premft.bmp", VIDEO_WIDTH, VIDEO_HEIGHT, 24, videoSample);

      // ----- Make Direct3D sample. -----
      IMFMediaBuffer* buf = NULL;
      DWORD bufLength = 0, lockedBufLength = 0;
      BYTE* pByteBuf = NULL;

      CHECK_HR(pVideoSample->ConvertToContiguousBuffer(&buf), "ConvertToContiguousBuffer failed.");
      CHECK_HR(buf->GetCurrentLength(&bufLength), "Get buffer length failed.");
      CHECK_HR(buf->Lock(&pByteBuf, NULL, &lockedBufLength), "Failed to lock sample buffer.");

      CHECK_HR(pD3DVideoSample->SetSampleTime(evrTimestamp), "Failed to set D3D video sample time.");
      CHECK_HR(pD3DVideoSample->SetSampleDuration(sampleDuration), "Failed to set D3D video sample duration.");
      CHECK_HR(pD3DVideoSample->GetBufferByIndex(0, &pDstBuffer), "Failed to get destination buffer.");
      CHECK_HR(pDstBuffer->QueryInterface(IID_PPV_ARGS(&p2DBuffer)), "Failed to get pointer to 2D buffer.");
      CHECK_HR(p2DBuffer->ContiguousCopyFrom(pByteBuf, bufLength), "Failed to copy D2D buffer (check the source media type matches the EVR input type).");

      CHECK_HR(buf->Unlock(), "Failed to unlock source buffer.");

      //CHECK_HR(videoSample->GetUINT32(MFSampleExtension_FrameCorruption, &uiAttribute), "Failed to get frame corruption attribute.");
      //CHECK_HR(pD3DVideoSample->SetUINT32(MFSampleExtension_FrameCorruption, uiAttribute), "Failed to set frame corruption attribute.");
      CHECK_HR(pVideoSample->GetUINT32(MFSampleExtension_Discontinuity, &uiAttribute), "Failed to get discontinuity attribute.");
      CHECK_HR(pD3DVideoSample->SetUINT32(MFSampleExtension_Discontinuity, uiAttribute), "Failed to set discontinuity attribute.");
      CHECK_HR(pVideoSample->GetUINT32(MFSampleExtension_CleanPoint, &uiAttribute), "Failed to get clean point attribute.");
      CHECK_HR(pD3DVideoSample->SetUINT32(MFSampleExtension_CleanPoint, uiAttribute), "Failed to set clean point attribute.");

      CHECK_HR(pStreamSink->ProcessSample(pD3DVideoSample), "Streamsink process sample failed.");

      SAFE_RELEASE(buf);

      evrTimestamp += sampleDuration;
    }

    SAFE_RELEASE(p2DBuffer);
    SAFE_RELEASE(pDstBuffer);
    SAFE_RELEASE(pVideoSample);
  }

done:

  printf("finished.\n");
  auto c = getchar();

  SAFE_RELEASE(pVideoReader);
  SAFE_RELEASE(pVideoSourceOutputType);
  SAFE_RELEASE(pSourceReaderType);
  SAFE_RELEASE(pWebCamMatchingType);
  SAFE_RELEASE(pImfEvrSinkType);
  SAFE_RELEASE(pHintMediaType);
  SAFE_RELEASE(pVideoSink);
  SAFE_RELEASE(pStreamSink);
  SAFE_RELEASE(pSinkWriter);
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

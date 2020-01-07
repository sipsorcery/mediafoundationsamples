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
* Status:
* File (mp4) source working. Webcam source not working.
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
#define WEBCAM_DEVICE_INDEX 0	  // Adjust according to desired video capture device.
#define USE_WEBCAM_SOURCE 1    // Set to 0 to use a file as the video source. Set to 1 to use webcam source. 

// Forward function definitions.
DWORD InitializeWindow(LPVOID lpThreadParameter);
HRESULT GetVideoSourceFromFile(LPWSTR path, IMFMediaSource** ppVideoSource, IMFSourceReader** ppVideoReader);
HRESULT GetVideoSourceFromDevice(UINT nDevice, IMFMediaSource** ppVideoSource, IMFSourceReader** ppVideoReader);
HRESULT GetSupportedMediaType(IMFMediaTypeHandler* pSinkMediaTypeHandler, IMFMediaType** pMediaType);

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

  IUnknown* colorConvTransformUnk = NULL;
  IMFTransform* pColorConvTransform = NULL; // This is colour converter MFT is used to convert between RGB32 and RGB24.
  IMFMediaType* pDecInputMediaType = NULL, * pDecOutputMediaType = NULL;
  IMFMediaType* pVideoSourceOutType = NULL;

  CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
    "COM initialisation failed.");

  CHECK_HR(MFStartup(MF_VERSION),
    "Media Foundation initialisation failed.");

  //CHECK_HR(ListCaptureDevices(DeviceType::Video), 
  //  "Error listing video capture devices.");

  // Need the color converter DSP for conversions between YUV, RGB etc.
  CHECK_HR(MFTRegisterLocalByCLSID(
    __uuidof(CColorConvertDMO),
    MFT_CATEGORY_VIDEO_PROCESSOR,
    L"",
    MFT_ENUM_FLAG_SYNCMFT,
    0,
    NULL,
    0,
    NULL),
    "Error registering colour converter DSP.");

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

  // ----- Set up Video source (is either a file or webcam capture device). -----

#if USE_WEBCAM_SOURCE
  CHECK_HR(GetVideoSourceFromDevice(WEBCAM_DEVICE_INDEX, &pVideoSource, &pVideoReader),
    "Failed to get webcam video source.");
#else
  CHECK_HR(GetVideoSourceFromFile(MEDIA_FILE_PATH, &pVideoSource, &pVideoReader),
    "Failed to get file video source.");
#endif

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

    // Set the video output type on the Webcam source.
  CHECK_HR(MFCreateMediaType(&pVideoSourceOutType), "Failed to create video output media type.");
  CHECK_HR(pVideoSourceOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set video output media major type.");
  CHECK_HR(pVideoSourceOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24), "Failed to set video sub-type attribute on media type.");
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

  // ----- Create an MFT to convert between RGB24 and RGB32. -----

  CHECK_HR(CoCreateInstance(CLSID_CColorConvertDMO, NULL, CLSCTX_INPROC_SERVER,
    IID_IUnknown, (void**)&colorConvTransformUnk),
    "Failed to create colour converter MFT.");

  CHECK_HR(colorConvTransformUnk->QueryInterface(IID_PPV_ARGS(&pColorConvTransform)),
    "Failed to get IMFTransform interface from colour converter MFT object.");

  MFCreateMediaType(&pDecInputMediaType);
  CHECK_HR(pVideoSourceOutType->CopyAllItems(pDecInputMediaType), "Error copying media type attributes to colour converter input media type.");
  CHECK_HR(pColorConvTransform->SetInputType(0, pDecInputMediaType, 0), "Failed to set input media type on colour converter MFT.");

  // The output from the transform is an exact copy of the media type set as the input for the EVR stream sink.
  MFCreateMediaType(&pDecOutputMediaType);
  CHECK_HR(pImfEvrSinkType->CopyAllItems(pDecOutputMediaType), "Error copying media type attributes to colour converter output media type.");
  CHECK_HR(pColorConvTransform->SetOutputType(0, pDecOutputMediaType, 0), "Failed to set output media type on colour converter MFT.");

  DWORD mftStatus = 0;
  CHECK_HR(pColorConvTransform->GetInputStatus(0, &mftStatus), "Failed to get input status from colour converter MFT.");
  if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
    printf("Colour converter MFT is not accepting data.\n");
    goto done;
  }

  CHECK_HR(pColorConvTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL), "Failed to process FLUSH command on colour converter MFT.");
  CHECK_HR(pColorConvTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), "Failed to process BEGIN_STREAMING command on colour converter MFT.");
  CHECK_HR(pColorConvTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), "Failed to process START_OF_STREAM command on colour converter MFT.");

  //CHECK_HR(GetSupportedMediaType(pMediaTypeHandler, &pImfEvrSinkType),
  //  "Failed to get supported media type.");

  /* auto doesSinkSupport = pSinkMediaTypeHandler->IsMediaTypeSupported(pImfEvrSinkType, &pHintMediaType);
    if (doesSinkSupport != S_OK) {
      std::cout << "Sink does not support desired media type." << std::endl;
      goto done;
    }
    else {
      CHECK_HR(pSinkMediaTypeHandler->SetCurrentMediaType(pImfEvrSinkType),
        "Failed to set input media type on EVR sink.");
    }*/

    // The block below always failed furing testing. My guess is the source media type handler
    // is not aligned with the video reader somehow.
    /*auto doesSrcSupport = pSourceMediaTypeHandler->IsMediaTypeSupported(pImfEvrSinkType, &pHintMediaType);
    if (doesSrcSupport != S_OK) {
      std::cout << "Source does not support desired media type." << std::endl;
      goto done;
    }
    else {
      CHECK_HR(pSourceMediaTypeHandler->SetCurrentMediaType(pImfEvrSinkType),
        "Failed to set output media type on source reader.");
    }*/

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
  IMFSample* videoSample = NULL;
  IMFMediaBuffer* pDstBuffer = NULL;
  IMF2DBuffer* p2DBuffer = NULL;
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
      DWORD mftOutFlags;

      // ----- Video source sample. -----

      CHECK_HR(videoSample->SetSampleTime(llTimeStamp), "Error setting the video sample time.");
      CHECK_HR(videoSample->GetCount(&sampleCount), "Failed to get video sample count.");
      CHECK_HR(videoSample->GetSampleDuration(&sampleDuration), "Failed to get video sample duration.");

      printf("Attempting to convert sample, sample count %d, sample duration %llu, sample time %llu.\n", sampleCount, sampleDuration, llTimeStamp);

      //CreateBitmapFromSample(L"capture_premft.bmp", VIDEO_WIDTH, VIDEO_HEIGHT, 24, videoSample);

      // ----- Apply colour conversion transfrom. -----

      CHECK_HR(pColorConvTransform->ProcessInput(0, videoSample, NULL), "The colour conversion decoder ProcessInput call failed.");

      IMFSample* mftOutSample = NULL;
      IMFMediaBuffer* mftBuffer = NULL;
      MFT_OUTPUT_STREAM_INFO StreamInfo;
      MFT_OUTPUT_DATA_BUFFER outputDataBuffer;
      DWORD processOutputStatus = 0;

      CHECK_HR(pColorConvTransform->GetOutputStreamInfo(0, &StreamInfo), "Failed to get output stream info from colour conversion MFT.");
      CHECK_HR(MFCreateSample(&mftOutSample), "Failed to create MF sample.");
      CHECK_HR(MFCreateMemoryBuffer(StreamInfo.cbSize, &mftBuffer), "Failed to create memory buffer.");
      CHECK_HR(mftOutSample->AddBuffer(mftBuffer), "Failed to add sample to buffer.");
      outputDataBuffer.dwStreamID = 0;
      outputDataBuffer.dwStatus = 0;
      outputDataBuffer.pEvents = NULL;
      outputDataBuffer.pSample = mftOutSample;

      auto mftProcessOutput = pColorConvTransform->ProcessOutput(0, 1, &outputDataBuffer, &processOutputStatus);

      if (mftProcessOutput == S_OK)
      {
        // ----- Make Direct3D sample. -----
        IMFMediaBuffer* buf = NULL;
        DWORD bufLength = 0, pByteBufLength = 0;
        BYTE* pByteBuf = NULL;

        CHECK_HR(mftOutSample->ConvertToContiguousBuffer(&buf), "ConvertToContiguousBuffer failed.");
        CHECK_HR(buf->GetCurrentLength(&bufLength), "Get buffer length failed.");
        CHECK_HR(buf->Lock(&pByteBuf, NULL, &pByteBufLength), "Failed to lock sample buffer.");

        printf("Color converted sample size %u.\n", bufLength);

        //CreateBitmapFile(L"capture_postmft.bmp", VIDEO_WIDTH, VIDEO_HEIGHT, 32, pByteBuf, pByteBufLength);

        CHECK_HR(pD3DVideoSample->SetSampleTime(llTimeStamp), "Failed to set D3D video sample time.");
        CHECK_HR(pD3DVideoSample->SetSampleDuration(sampleDuration), "Failed to set D3D video sample duration.");
        CHECK_HR(pD3DVideoSample->GetBufferByIndex(0, &pDstBuffer), "Failed to get destination buffer.");
        CHECK_HR(pDstBuffer->QueryInterface(IID_PPV_ARGS(&p2DBuffer)), "Failed to get pointer to 2D buffer.");
        CHECK_HR(p2DBuffer->ContiguousCopyFrom(pByteBuf, pByteBufLength), "Failed to copy D2D buffer.");

        /*CHECK_HR(videoSample->GetUINT32(MFSampleExtension_FrameCorruption, &uiAttribute), "Failed to get frame corruption attribute.");
        CHECK_HR(pD3DVideoSample->SetUINT32(MFSampleExtension_FrameCorruption, uiAttribute), "Failed to set frame corruption attribute.");
        CHECK_HR(videoSample->GetUINT32(MFSampleExtension_Discontinuity, &uiAttribute), "Failed to get discontinuity attribute.");
        CHECK_HR(pD3DVideoSample->SetUINT32(MFSampleExtension_Discontinuity, uiAttribute), "Failed to set discontinuity attribute.");
        CHECK_HR(videoSample->GetUINT32(MFSampleExtension_CleanPoint, &uiAttribute), "Failed to get clean point attribute.");
        CHECK_HR(pD3DVideoSample->SetUINT32(MFSampleExtension_CleanPoint, uiAttribute), "Failed to set clean point attribute.");*/

        CHECK_HR(pStreamSink->ProcessSample(pD3DVideoSample), "Streamsink process sample failed.");

        CHECK_HR(buf->Unlock(), "Failed to unlock source buffer.");

        SAFE_RELEASE(buf);
        SAFE_RELEASE(mftOutSample);
        SAFE_RELEASE(mftBuffer);
      }
      else {
        printf("Colour conversion failed with %.2X.\n", mftProcessOutput);
        break;
      }
     
      Sleep(sampleDuration / 10000); // Duration is given in 10's of nano seconds.
    }

    SAFE_RELEASE(p2DBuffer);
    SAFE_RELEASE(pDstBuffer);
    SAFE_RELEASE(videoSample);
  }

done:

  printf("finished.\n");
  auto c = getchar();

  SAFE_RELEASE(pVideoReader);
  SAFE_RELEASE(videoSourceOutputType);
  SAFE_RELEASE(pvideoSourceModType);
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
* Iterates the sink media's available type in an attempt to find
* one it is happy with,
* @param[in] SinkMediaTypeHandler: the sink media handler to find a matching media type for.
* @param[out] ppMediaType: will be set with a media type if successful.
* @@Returns S_OK if successful or an error code if not.
*/
HRESULT GetSupportedMediaType(IMFMediaTypeHandler* pSinkMediaTypeHandler, IMFMediaType** ppMediaType)
{
  IMFMediaType* pSupportedType = NULL;
  DWORD sourceMediaTypeCount = 0;
  HRESULT hr = S_OK;

  hr = pSinkMediaTypeHandler->GetMediaTypeCount(&sourceMediaTypeCount);
  CHECK_HR(hr, "Error getting sink media type count.");

  // Find a media type that the sink and its writer support.
  for (UINT i = 0; i < sourceMediaTypeCount; i++)
  {
    hr = pSinkMediaTypeHandler->GetMediaTypeByIndex(i, &pSupportedType);
    CHECK_HR(hr, "Error getting media type from sink media type handler.");

    std::cout << GetMediaTypeDescription(pSupportedType) << std::endl;

    if (pSinkMediaTypeHandler->IsMediaTypeSupported(pSupportedType, NULL) == S_OK) {
      std::cout << "Matching media type found." << std::endl;
      ppMediaType = &pSupportedType;
      break;
    }
    else {
      std::cout << "Source media type does not match." << std::endl;
      SAFE_RELEASE(pSupportedType);
    }
  }

done:
  return hr;
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
* Gets a video source reader from a device such as a webcam.
* @param[in] nDevice: the video device index to attempt to get the source reader for.
* @param[out] ppVideoSource: will be set with the source for the reader if successful.
* @param[out] ppVideoReader: will be set with the reader if successful.
* @@Returns S_OK if successful or an error code if not.
*/
HRESULT GetVideoSourceFromDevice(UINT nDevice, IMFMediaSource** ppVideoSource, IMFSourceReader** ppVideoReader)
{
  UINT32 videoDeviceCount = 0;
  IMFAttributes* videoConfig = NULL;
  IMFActivate** videoDevices = NULL;
  WCHAR* webcamFriendlyName;
  UINT nameLength = 0;

  HRESULT hr = S_OK;

  // Get the first available webcam.
  hr = MFCreateAttributes(&videoConfig, 1);
  CHECK_HR(hr, "Error creating video configuation.");

  // Request video capture devices.
  hr = videoConfig->SetGUID(
    MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
    MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
  CHECK_HR(hr, "Error initialising video configuration object.");

  hr = MFEnumDeviceSources(videoConfig, &videoDevices, &videoDeviceCount);
  CHECK_HR(hr, "Error enumerating video devices.");

  hr = videoDevices[nDevice]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &webcamFriendlyName, &nameLength);
  CHECK_HR(hr, "Error retrieving vide device friendly name.\n");

  wprintf(L"First available webcam: %s\n", webcamFriendlyName);

  hr = videoDevices[nDevice]->ActivateObject(IID_PPV_ARGS(ppVideoSource));
  CHECK_HR(hr, "Error activating video device.");

  // Create a source reader.
  hr = MFCreateSourceReaderFromMediaSource(
    *ppVideoSource,
    videoConfig,
    ppVideoReader);
  CHECK_HR(hr, "Error creating video source reader.");

done:

  SAFE_RELEASE(videoConfig);
  SAFE_RELEASE(videoDevices);

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

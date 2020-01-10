/******************************************************************************
* Filename: MFVideoEVRWebcamMFT.cpp
*
* Description:
* This file contains a C++ console application that plays the
* video stream from a webcam source on the Enhanced Video Renderer.
* 
* The difference between this sample and the MFVideoEVRWebcam sample is that
* this one manually wires up the colour converter Transform. Turns out this
* isn't required if the source reader is created with the attribute
* MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING set. That was only discovered 
* afterwards and this sample still serves as a useful demonstration of how
* to wire up an MFT transform.
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 10 Jan 2020	  Aaron Clauson	  Created, Dublin, Ireland.
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
#define VIDEO_FRAME_RATE 30
#define WEBCAM_DEVICE_INDEX 0	  // Adjust according to desired video capture device.

// Forward function definitions.
DWORD InitializeWindow(LPVOID lpThreadParameter);
HRESULT GetVideoSourceFromDevice(UINT nDevice, IMFMediaSource** ppVideoSource, IMFSourceReader** ppVideoReader);

// Constants 
const WCHAR CLASS_NAME[] = L"MFVideoEVRWebcam Window Class";
const WCHAR WINDOW_NAME[] = L"MFVideoEVRWebcam";

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
  IMFTransform* pColorConvTransform = NULL; // This is colour converter MFT is used to convert between the webcam pixel format and RGB32.
  IMFMediaType* pDecInputMediaType = NULL, * pDecOutputMediaType = NULL;
  IMFMediaType* pWebcamSourceType = NULL;

  CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
    "COM initialisation failed.");

  CHECK_HR(MFStartup(MF_VERSION),
    "Media Foundation initialisation failed.");

  /*CHECK_HR(ListCaptureDevices(DeviceType::Video), 
    "Error listing video capture devices.");*/

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

  // ----- Set up webcam video source. -----

  CHECK_HR(GetVideoSourceFromDevice(WEBCAM_DEVICE_INDEX, &pVideoSource, &pVideoReader),
    "Failed to get webcam video source.");

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

  // Set the video input type on the EVR sink.
  CHECK_HR(MFCreateMediaType(&pImfEvrSinkType), "Failed to create video output media type.");
  CHECK_HR(pImfEvrSinkType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set video output media major type.");
  CHECK_HR(pImfEvrSinkType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32), "Failed to set video sub-type attribute on media type.");
  CHECK_HR(pImfEvrSinkType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive), "Failed to set interlace mode attribute on media type.");
  CHECK_HR(pImfEvrSinkType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE), "Failed to set independent samples attribute on media type.");
  CHECK_HR(MFSetAttributeRatio(pImfEvrSinkType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set pixel aspect ratio attribute on media type.");
  CHECK_HR(MFSetAttributeSize(pImfEvrSinkType, MF_MT_FRAME_SIZE, VIDEO_WIDTH, VIDEO_HEIGHT), "Failed to set the frame size attribute on media type.");
  CHECK_HR(MFSetAttributeSize(pImfEvrSinkType, MF_MT_FRAME_RATE, VIDEO_FRAME_RATE, 1), "Failed to set the frame rate attribute on media type.");
  CHECK_HR(CopyAttribute(videoSourceOutputType, pImfEvrSinkType, MF_MT_DEFAULT_STRIDE), "Failed to copy default stride attribute.");

  // My webcam supports the same media type as the EVR input type except for the pixel format.
  CHECK_HR(MFCreateMediaType(&pWebcamSourceType), "Failed to create webcam output media type.");
  CHECK_HR(pImfEvrSinkType->CopyAllItems(pWebcamSourceType), "Error copying media type attributes from EVR input to webcam output media type.");
  CHECK_HR(CopyAttribute(videoSourceOutputType, pWebcamSourceType, MF_MT_SUBTYPE), "Failed to set video sub-type attribute on webcam media type.");

  CHECK_HR(pSinkMediaTypeHandler->SetCurrentMediaType(pImfEvrSinkType),
    "Failed to set input media type on EVR sink.");

  CHECK_HR(pVideoReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pWebcamSourceType),
    "Failed to set output media type on source reader.");

  std::cout << "EVR input media type defined as:" << std::endl;
  std::cout << GetMediaTypeDescription(pImfEvrSinkType) << std::endl << std::endl;

  // ----- Create an MFT to convert between webcam pixel format and RGB32. -----

  CHECK_HR(CoCreateInstance(CLSID_CColorConvertDMO, NULL, CLSCTX_INPROC_SERVER,
    IID_IUnknown, (void**)&colorConvTransformUnk),
    "Failed to create colour converter MFT.");

  CHECK_HR(colorConvTransformUnk->QueryInterface(IID_PPV_ARGS(&pColorConvTransform)),
    "Failed to get IMFTransform interface from colour converter MFT object.");

  MFCreateMediaType(&pDecInputMediaType);
  CHECK_HR(pWebcamSourceType->CopyAllItems(pDecInputMediaType), "Error copying media type attributes to colour converter input media type.");
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

  LONGLONG evrTimestamp = 0;

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
      LONGLONG sampleDuration = 0;
      DWORD mftOutFlags;

      // ----- Video source sample. -----

      CHECK_HR(videoSample->SetSampleTime(llTimeStamp), "Error setting the video sample time.");
      CHECK_HR(videoSample->GetSampleDuration(&sampleDuration), "Failed to get video sample duration.");

      //printf("Attempting to convert sample, sample duration %llu, sample time %llu, evr timestamp %llu.\n", sampleDuration, llTimeStamp, evrTimestamp);

      // Relies on the webcam pixel format being RGB24.
      //CreateBitmapFromSample(L"capture_premft.bmp", VIDEO_WIDTH, VIDEO_HEIGHT, 24, videoSample);

      // ----- Apply colour conversion transfrom. -----

      CHECK_HR(pColorConvTransform->ProcessInput(0, videoSample, NULL), "The colour conversion decoder ProcessInput call failed.");

      IMFSample* mftOutSample = NULL;
      IMFMediaBuffer* mftOutBuffer = NULL;
      MFT_OUTPUT_STREAM_INFO StreamInfo;
      MFT_OUTPUT_DATA_BUFFER outputDataBuffer;
      DWORD processOutputStatus = 0;

      CHECK_HR(pColorConvTransform->GetOutputStreamInfo(0, &StreamInfo), "Failed to get output stream info from colour conversion MFT.");
      CHECK_HR(MFCreateSample(&mftOutSample), "Failed to create MF sample.");
      CHECK_HR(MFCreateMemoryBuffer(StreamInfo.cbSize, &mftOutBuffer), "Failed to create memory buffer.");
      CHECK_HR(mftOutSample->AddBuffer(mftOutBuffer), "Failed to add sample to buffer.");
      outputDataBuffer.dwStreamID = 0;
      outputDataBuffer.dwStatus = 0;
      outputDataBuffer.pEvents = NULL;
      outputDataBuffer.pSample = mftOutSample;

      auto mftProcessOutput = pColorConvTransform->ProcessOutput(0, 1, &outputDataBuffer, &processOutputStatus);

      //printf("Colour conversion result %.2X, MFT status %.2X.\n", mftProcessOutput, processOutputStatus);

      if (mftProcessOutput == S_OK)
      {
        // ----- Make Direct3D sample. -----
        IMFMediaBuffer* buf = NULL;
        DWORD bufLength = 0, pByteBufLength = 0;
        BYTE* pByteBuf = NULL;

        CHECK_HR(mftOutSample->ConvertToContiguousBuffer(&buf), "ConvertToContiguousBuffer failed.");
        CHECK_HR(buf->GetCurrentLength(&bufLength), "Get buffer length failed.");
        CHECK_HR(buf->Lock(&pByteBuf, NULL, &pByteBufLength), "Failed to lock sample buffer.");

        //printf("Color converted sample size %u.\n", bufLength);

        //CreateBitmapFile(L"capture_postmft.bmp", VIDEO_WIDTH, VIDEO_HEIGHT, 32, pByteBuf, pByteBufLength);

        //CHECK_HR(pD3DVideoSample->SetSampleTime(llTimeStamp), "Failed to set D3D video sample time.");
        CHECK_HR(pD3DVideoSample->SetSampleTime(evrTimestamp), "Failed to set D3D video sample time.");
        CHECK_HR(pD3DVideoSample->SetSampleDuration(sampleDuration), "Failed to set D3D video sample duration.");
        CHECK_HR(pD3DVideoSample->GetBufferByIndex(0, &pDstBuffer), "Failed to get destination buffer.");
        CHECK_HR(pDstBuffer->QueryInterface(IID_PPV_ARGS(&p2DBuffer)), "Failed to get pointer to 2D buffer.");
        CHECK_HR(p2DBuffer->ContiguousCopyFrom(pByteBuf, pByteBufLength), "Failed to copy D2D buffer.");

        //CHECK_HR(videoSample->GetUINT32(MFSampleExtension_FrameCorruption, &uiAttribute), "Failed to get frame corruption attribute.");
        //CHECK_HR(pD3DVideoSample->SetUINT32(MFSampleExtension_FrameCorruption, uiAttribute), "Failed to set frame corruption attribute.");
        CHECK_HR(videoSample->GetUINT32(MFSampleExtension_Discontinuity, &uiAttribute), "Failed to get discontinuity attribute.");
        CHECK_HR(pD3DVideoSample->SetUINT32(MFSampleExtension_Discontinuity, uiAttribute), "Failed to set discontinuity attribute.");
        CHECK_HR(videoSample->GetUINT32(MFSampleExtension_CleanPoint, &uiAttribute), "Failed to get clean point attribute.");
        CHECK_HR(pD3DVideoSample->SetUINT32(MFSampleExtension_CleanPoint, uiAttribute), "Failed to set clean point attribute.");

        CHECK_HR(pStreamSink->ProcessSample(pD3DVideoSample), "Streamsink process sample failed.");

        CHECK_HR(buf->Unlock(), "Failed to unlock source buffer.");

        SAFE_RELEASE(buf);
        SAFE_RELEASE(mftOutSample);
        SAFE_RELEASE(mftOutBuffer);
      }
      else {
        printf("Colour conversion failed with %.2X.\n", mftProcessOutput);
        break;
      }
     
      evrTimestamp += sampleDuration;
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

//-----------------------------------------------------------------------------
// Filename: main.cpp
//
// Description: Copy of sample from: 
// https://docs.microsoft.com/en-us/windows/win32/medfound/using-the-sample-grabber-sink
//
// Remarks:
// 21 Jan 2020  Aaron Clauson   Investigating why the guidMajorMediaType parameter
// doesn't get set on the OnProcessSample callback. CAn be worked around by creating
// two separate callback instances but seems like that shouldn't be necessary.
// https://stackoverflow.com/questions/59839187/sample-grabber-sink-not-setting-media-type-guid
//
//-----------------------------------------------------------------------------

#include "SampleGrabber.h"

#include <chrono>
#include <iostream>
#include <string>

HRESULT CreateMediaSource(PCWSTR pszURL, IMFMediaSource **ppSource);
HRESULT CreateTopology(IMFMediaSource *pSource, IMFActivate *pVideoSink, IMFActivate *pAudioSink, IMFTopology **ppTopo);
HRESULT RunSession(IMFMediaSession *pSession, IMFTopology *pTopology);
HRESULT RunSampleGrabber(PCWSTR pszFileName);

#define SOURCE_FILENAME L"../MediaFiles/big_buck_bunny.mp4"

int wmain(int argc, wchar_t* argv[])
{
  std::cout << "Sample Grabber test console starting..." << std::endl;

  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if(SUCCEEDED(hr))
  {
    hr = MFStartup(MF_VERSION);
    if(SUCCEEDED(hr))
    {
      std::wstring path = SOURCE_FILENAME;

      std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

      hr = RunSampleGrabber(path.c_str());

      std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

      std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;

      MFShutdown();
    }
    CoUninitialize();
  }
  return 0;
}

// Create a media source from a URL.
HRESULT CreateMediaSource(PCWSTR pszURL, IMFMediaSource **ppSource)
{
  IMFSourceResolver* pSourceResolver = NULL;
  IUnknown* pSource = NULL;

  // Create the source resolver.
  HRESULT hr = S_OK;
  CHECK_HR(hr = MFCreateSourceResolver(&pSourceResolver));

  MF_OBJECT_TYPE ObjectType;
  CHECK_HR(hr = pSourceResolver->CreateObjectFromURL(pszURL,
    MF_RESOLUTION_MEDIASOURCE, NULL, &ObjectType, &pSource));

  hr = pSource->QueryInterface(IID_PPV_ARGS(ppSource));

done:
  SafeRelease(&pSourceResolver);
  SafeRelease(&pSource);
  return hr;
}

HRESULT RunSampleGrabber(PCWSTR pszFileName)
{
  IMFMediaSession *pSession = NULL;
  IMFMediaSource *pSource = NULL;
  SampleGrabberCB *pAudioSampleGrabberSinkCallback = NULL;
  SampleGrabberCB* pVideoSampleGrabberSinkCallback = NULL;
  IMFActivate *pAudioSinkActivate = NULL, *pVideoSinkActivate = NULL;
  IMFTopology *pTopology = NULL;
  IMFMediaType *pVideoType = NULL, *pAudioType = NULL;

  // Configure the media type that the Sample Grabber will receive.
  // Setting the major and subtype is usually enough for the topology loader
  // to resolve the topology.
  HRESULT hr = S_OK;
  CHECK_HR(hr = MFCreateMediaType(&pVideoType));
  CHECK_HR(hr = pVideoType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
  CHECK_HR(hr = pVideoType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_I420));

  CHECK_HR(hr = MFCreateMediaType(&pAudioType));
  CHECK_HR(hr = pAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
  CHECK_HR(hr = pAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM));
  CHECK_HR(hr = pAudioType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1));
  CHECK_HR(hr = pAudioType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16));
  CHECK_HR(hr = pAudioType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 8000));

  // Create the sample grabber sink.
  CHECK_HR(hr = SampleGrabberCB::CreateInstance(&pAudioSampleGrabberSinkCallback, MFMediaType_Audio));
  CHECK_HR(hr = SampleGrabberCB::CreateInstance(&pVideoSampleGrabberSinkCallback, MFMediaType_Video));
  CHECK_HR(hr = MFCreateSampleGrabberSinkActivate(pAudioType, pAudioSampleGrabberSinkCallback, &pAudioSinkActivate));
  CHECK_HR(hr = MFCreateSampleGrabberSinkActivate(pVideoType, pVideoSampleGrabberSinkCallback, &pVideoSinkActivate));

  // To run as fast as possible, set this attribute (requires Windows 7):
  //CHECK_HR(hr = pSinkActivate->SetUINT32(MF_SAMPLEGRABBERSINK_IGNORE_CLOCK, TRUE));

  // Create the Media Session.
  CHECK_HR(hr = MFCreateMediaSession(NULL, &pSession));

  // Create the media source.
  CHECK_HR(hr = CreateMediaSource(pszFileName, &pSource));

  // Create the topology.
  CHECK_HR(hr = CreateTopology(pSource, pVideoSinkActivate, pAudioSinkActivate, &pTopology));

  // Run the media session.
  CHECK_HR(hr = RunSession(pSession, pTopology));

done:
  // Clean up.
  if(pSource)
  {
    pSource->Shutdown();
  }
  if(pSession)
  {
    pSession->Shutdown();
  }

  SafeRelease(&pSession);
  SafeRelease(&pSource);
  SafeRelease(&pAudioSampleGrabberSinkCallback);
  SafeRelease(&pVideoSampleGrabberSinkCallback);
  SafeRelease(&pAudioSinkActivate);
  SafeRelease(&pVideoSinkActivate);
  SafeRelease(&pTopology);
  SafeRelease(&pVideoType);
  SafeRelease(&pAudioType);
  return hr;
}

// Add a source node to a topology.
HRESULT AddSourceNode(
  IMFTopology *pTopology,           // Topology.
  IMFMediaSource *pSource,          // Media source.
  IMFPresentationDescriptor *pPD,   // Presentation descriptor.
  IMFStreamDescriptor *pSD,         // Stream descriptor.
  IMFTopologyNode **ppNode)         // Receives the node pointer.
{
  IMFTopologyNode *pNode = NULL;

  HRESULT hr = S_OK;
  CHECK_HR(hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pNode));
  CHECK_HR(hr = pNode->SetUnknown(MF_TOPONODE_SOURCE, pSource));
  CHECK_HR(hr = pNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD));
  CHECK_HR(hr = pNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSD));
  CHECK_HR(hr = pTopology->AddNode(pNode));

  // Return the pointer to the caller.
  *ppNode = pNode;
  (*ppNode)->AddRef();

done:
  SafeRelease(&pNode);
  return hr;
}

// Add an output node to a topology.
HRESULT AddOutputNode(
  IMFTopology *pTopology,     // Topology.
  IMFActivate *pActivate,     // Media sink activation object.
  DWORD dwId,                 // Identifier of the stream sink.
  IMFTopologyNode **ppNode)   // Receives the node pointer.
{
  IMFTopologyNode *pNode = NULL;

  HRESULT hr = S_OK;
  CHECK_HR(hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pNode));
  CHECK_HR(hr = pNode->SetObject(pActivate));
  CHECK_HR(hr = pNode->SetUINT32(MF_TOPONODE_STREAMID, dwId));
  CHECK_HR(hr = pNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE));
  CHECK_HR(hr = pTopology->AddNode(pNode));

  // Return the pointer to the caller.
  *ppNode = pNode;
  (*ppNode)->AddRef();

done:
  SafeRelease(&pNode);
  return hr;
}

// Create the topology.
HRESULT CreateTopology(IMFMediaSource *pSource, IMFActivate *pVideoSinkActivate, IMFActivate *pAudioSinkActivate, IMFTopology **ppTopo)
{
  IMFTopology *pTopology = NULL;
  IMFPresentationDescriptor *pPD = NULL;
  IMFStreamDescriptor *pSD = NULL;
  IMFMediaTypeHandler *pHandler = NULL;
  IMFTopologyNode *pAudioSourceNode = NULL, *pVideoSourceNode = NULL;
  IMFTopologyNode *pAudioSinkNode = NULL, *pVideoSinkNode = NULL;

  HRESULT hr = S_OK;
  DWORD cStreams = 0;

  CHECK_HR(hr = MFCreateTopology(&pTopology));
  CHECK_HR(hr = pSource->CreatePresentationDescriptor(&pPD));
  CHECK_HR(hr = pPD->GetStreamDescriptorCount(&cStreams));

  for(DWORD i = 0; i < cStreams; i++)
  {
    // In this example, we look for audio streams and connect them to the sink.

    BOOL fSelected = FALSE;
    GUID majorType;

    CHECK_HR(hr = pPD->GetStreamDescriptorByIndex(i, &fSelected, &pSD));
    CHECK_HR(hr = pSD->GetMediaTypeHandler(&pHandler));
    CHECK_HR(hr = pHandler->GetMajorType(&majorType));

    if(majorType == MFMediaType_Video && fSelected)
    {
      CHECK_HR(hr = AddSourceNode(pTopology, pSource, pPD, pSD, &pVideoSourceNode));
      CHECK_HR(hr = AddOutputNode(pTopology, pVideoSinkActivate, 0, &pVideoSinkNode));
      CHECK_HR(hr = pVideoSourceNode->ConnectOutput(0, pVideoSinkNode, 0));
    }
    else if(majorType == MFMediaType_Audio && fSelected)
    {
      CHECK_HR(hr = AddSourceNode(pTopology, pSource, pPD, pSD, &pAudioSourceNode));
      CHECK_HR(hr = AddOutputNode(pTopology, pAudioSinkActivate, 0, &pAudioSinkNode));
      CHECK_HR(hr = pAudioSourceNode->ConnectOutput(0, pAudioSinkNode, 0));
    }
    else
    {
      CHECK_HR(hr = pPD->DeselectStream(i));
    }

    SafeRelease(&pSD);
    SafeRelease(&pHandler);
  }

  *ppTopo = pTopology;
  (*ppTopo)->AddRef();

done:
  SafeRelease(&pTopology);
  SafeRelease(&pAudioSourceNode);
  SafeRelease(&pVideoSourceNode);
  SafeRelease(&pAudioSinkNode);
  SafeRelease(&pVideoSinkNode);
  SafeRelease(&pPD);
  SafeRelease(&pSD);
  SafeRelease(&pHandler);
  return hr;
}

HRESULT RunSession(IMFMediaSession *pSession, IMFTopology *pTopology)
{
  IMFMediaEvent *pEvent = NULL;

  PROPVARIANT var;
  PropVariantInit(&var);

  HRESULT hr = S_OK;
  CHECK_HR(hr = pSession->SetTopology(0, pTopology));
  CHECK_HR(hr = pSession->Start(&GUID_NULL, &var));

  while(1)
  {
    HRESULT hrStatus = S_OK;
    MediaEventType met;

    CHECK_HR(hr = pSession->GetEvent(0, &pEvent));
    CHECK_HR(hr = pEvent->GetStatus(&hrStatus));
    CHECK_HR(hr = pEvent->GetType(&met));

    if(FAILED(hrStatus))
    {
      printf("Session error: 0x%x (event id: %d)\n", hrStatus, met);
      hr = hrStatus;
      goto done;
    }
    if(met == MESessionEnded)
    {
      break;
    }
    SafeRelease(&pEvent);
  }

done:
  SafeRelease(&pEvent);
  return hr;
}
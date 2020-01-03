#pragma once

#include <new>
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <Shlwapi.h>
#include <stdio.h>

#pragma comment(lib, "mfplat")
#pragma comment(lib, "mf")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "Shlwapi")

template <class T> void SafeRelease(T **ppT)
{
  if(*ppT)
  {
    (*ppT)->Release();
    *ppT = NULL;
  }
}

#define CHECK_HR(x) if (FAILED(x)) { goto done; }

// The class that implements the callback interface.
class SampleGrabberCB: public IMFSampleGrabberSinkCallback
{
  long m_cRef;

  SampleGrabberCB(): m_cRef(1) {}

public:
  static HRESULT CreateInstance(SampleGrabberCB **ppCB);

  // IUnknown methods
  STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
  STDMETHODIMP_(ULONG) AddRef();
  STDMETHODIMP_(ULONG) Release();

  // IMFClockStateSink methods
  STDMETHODIMP OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset);
  STDMETHODIMP OnClockStop(MFTIME hnsSystemTime);
  STDMETHODIMP OnClockPause(MFTIME hnsSystemTime);
  STDMETHODIMP OnClockRestart(MFTIME hnsSystemTime);
  STDMETHODIMP OnClockSetRate(MFTIME hnsSystemTime, float flRate);

  // IMFSampleGrabberSinkCallback methods
  STDMETHODIMP OnSetPresentationClock(IMFPresentationClock* pClock);
  STDMETHODIMP OnProcessSample(REFGUID guidMajorMediaType, DWORD dwSampleFlags,
    LONGLONG llSampleTime, LONGLONG llSampleDuration, const BYTE * pSampleBuffer,
    DWORD dwSampleSize);
  STDMETHODIMP OnShutdown();
};
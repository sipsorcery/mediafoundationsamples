#include "SampleGrabber.h"

// SampleGrabberCB implementation

void printf_guid(REFGUID guid) {
  printf("Guid = {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\n",
    guid.Data1, guid.Data2, guid.Data3,
    guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
    guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}

// Create a new instance of the object.
HRESULT SampleGrabberCB::CreateInstance(SampleGrabberCB **ppCB, REFGUID majorMediaType)
{
  *ppCB = new (std::nothrow) SampleGrabberCB();

  if(ppCB == NULL)
  {
    return E_OUTOFMEMORY;
  }

  (*ppCB)->_majorMediaType = majorMediaType;

  return S_OK;
}

STDMETHODIMP SampleGrabberCB::QueryInterface(REFIID riid, void** ppv)
{
  static const QITAB qit[] =
  {
    QITABENT(SampleGrabberCB, IMFSampleGrabberSinkCallback),
    QITABENT(SampleGrabberCB, IMFClockStateSink),
  {0}
  };
  return QISearch(this, qit, riid, ppv);
}

STDMETHODIMP_(ULONG) SampleGrabberCB::AddRef()
{
  return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) SampleGrabberCB::Release()
{
  ULONG cRef = InterlockedDecrement(&m_cRef);
  if(cRef == 0)
  {
    delete this;
  }
  return cRef;

}

// IMFClockStateSink methods.

// In these example, the IMFClockStateSink methods do not perform any actions. 
// You can use these methods to track the state of the sample grabber sink.

STDMETHODIMP SampleGrabberCB::OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset)
{
  return S_OK;
}

STDMETHODIMP SampleGrabberCB::OnClockStop(MFTIME hnsSystemTime)
{
  return S_OK;
}

STDMETHODIMP SampleGrabberCB::OnClockPause(MFTIME hnsSystemTime)
{
  return S_OK;
}

STDMETHODIMP SampleGrabberCB::OnClockRestart(MFTIME hnsSystemTime)
{
  return S_OK;
}

STDMETHODIMP SampleGrabberCB::OnClockSetRate(MFTIME hnsSystemTime, float flRate)
{
  return S_OK;
}

// IMFSampleGrabberSink methods.

STDMETHODIMP SampleGrabberCB::OnSetPresentationClock(IMFPresentationClock* pClock)
{
  return S_OK;
}

STDMETHODIMP SampleGrabberCB::OnProcessSample(REFGUID guidMajorMediaType, DWORD dwSampleFlags,
  LONGLONG llSampleTime, LONGLONG llSampleDuration, const BYTE * pSampleBuffer,
  DWORD dwSampleSize)
{
  // Display information about the sample.
  //printf_guid(guidMajorMediaType);
  printf_guid(_majorMediaType);
  printf("Sample: start = %I64d, duration = %I64d, bytes = %d\n", llSampleTime, llSampleDuration, dwSampleSize);
  return S_OK;
}

STDMETHODIMP SampleGrabberCB::OnShutdown()
{
  return S_OK;
}

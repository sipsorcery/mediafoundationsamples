#include "pch.h"
#include "Mp4Sampler.h"

namespace SurfaceGenerator
{
	Mp4Sampler::Mp4Sampler()
	{
	}

	void Mp4Sampler::Initialise(Platform::String^ path)
	{
		HRESULT hr = S_OK;
		IMFSourceResolver *pSourceResolver = nullptr;
		MF_OBJECT_TYPE objectType = MF_OBJECT_INVALID;
		IUnknown* uSource = nullptr;
		IMFMediaSource *mediaFileSource = nullptr;

		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		MFStartup(MF_VERSION);

		CHECK_HR(hr = MFCreateSourceResolver(&pSourceResolver), "MFCreateSourceResolver failed.\n");
		
		OutputDebugStringW(L"Attempting to creafe MF source reader for ");
		OutputDebugStringW(path->Begin());
		OutputDebugStringW(L"\n");
		
		CHECK_HR(hr = pSourceResolver->CreateObjectFromURL(
			//L"E:\\Data\\LiveMesh\\Source\\mediafoundationsamples\\MediaFiles\\big_buck_bunny.mp4",		// URL of the source.
			path->Begin(),
			//L"rtsp://10.1.1.2/slamtv60.264",
			MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
			NULL,                       // Optional property store.
			&objectType,				// Receives the created object type. 
			&uSource					// Receives a pointer to the media source.
			), "Failed to create media source resolver for file.\n");

		CHECK_HR(uSource->QueryInterface(IID_PPV_ARGS(&mediaFileSource)), "Failed to create media file source.\n");

		CHECK_HR(MFCreateSourceReaderFromMediaSource(mediaFileSource, NULL, &_videoReader),	"Error creating media source reader.\n");

		return;

	done:

		throw Platform::Exception::CreateException(hr, "Mp4Sample Initialise failed.");
	}
	
	void Mp4Sampler::GetSample(Windows::Media::Core::MediaStreamSourceSampleRequest ^ request)
	{
		Microsoft::WRL::ComPtr<IMFMediaStreamSourceSampleRequest> spRequest;
		HRESULT hr = S_OK;
		IMFSample *videoSample = NULL;
		DWORD streamIndex, flags;
		LONGLONG llTimeStamp;

		CHECK_HR(hr = reinterpret_cast<IInspectable*>(request)->QueryInterface(spRequest.ReleaseAndGetAddressOf()), "Failed to get MF interface for media sample request.\n");

		while (true)
		{
			CHECK_HR(hr = _videoReader->ReadSample(
				MF_SOURCE_READER_FIRST_VIDEO_STREAM,
				0,                              // Flags.
				&streamIndex,                   // Receives the actual stream index. 
				&flags,                         // Receives status flags.
				&llTimeStamp,					// Receives the time stamp.
				&videoSample                    // Receives the sample or NULL.
				), "Error reading video sample.");

			if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
			{
				OutputDebugStringW(L"End of stream.\n");
				return;
			}
			if (flags & MF_SOURCE_READERF_STREAMTICK)
			{
				OutputDebugStringW(L"Stream tick.\n");
			}

			if (!videoSample)
			{
				OutputDebugStringW(L"Null video sample.\n");
			}
			else
			{
				OutputDebugStringW(L"Attempting to write sample to stream sink.\n");

				CHECK_HR(hr = videoSample->SetSampleTime(llTimeStamp), "Error setting the video sample time.\n");

				CHECK_HR(hr = spRequest->SetSample(videoSample), "Error setting sample on media sample request.\n");

				return;
			}
		}

		done:

			throw Platform::Exception::CreateException(hr, "Mp4Sample GetSample failed.");

	}
}

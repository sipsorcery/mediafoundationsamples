/// Filename: MFAudio.cpp
///
/// Description:
/// This file contains a C++ console application that is attempting to play the audio stream from a sample MP4 file using the Windows
/// Media Foundation API. Specifically it's attempting to use the Streaming Audio Renderer (https://msdn.microsoft.com/en-us/library/windows/desktop/aa369729%28v=vs.85%29.aspx).
/// to playback the video.
///
/// NOTE: This sample is "sort of" working. The audio is played back but it is stuttering. The problem is something to do with the sampling rate. If I use ffmpeg to change the 
/// sampling rate from 22,050 samples/second to 48,000 samples/second then the audio playback is smooth. My understanding is that the Media Foundation pipeline is supposed to 
/// take care of the resampling automatically. I have experimented with adding Resampler Media Transfrom to the playback pipeline but that did not fix the problem. Still a work
// in progress.
///
/// History:
/// 01 Jan 2015	Aaron Clauson (aaron@sipsorcery.com)	Created.
///
/// License: Public

#include <stdio.h>
#include <tchar.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <mferror.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#define CHECK_HR(hr, msg) if (hr != S_OK) { printf(msg); printf("Error: %.2X.\n", hr); goto done; }

int _tmain(int argc, _TCHAR* argv[])
{
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	MFStartup(MF_VERSION);

	IMFSourceResolver *pSourceResolver = NULL;
	IUnknown* uSource = NULL;
	IMFMediaSource *mediaFileSource = NULL;
	IMFSourceReader *pSourceReader = NULL;
	IMFMediaType *pAudioOutType = NULL;
	IMFMediaType *pFileAudioMediaType = NULL;
	MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;
	IMFMediaSink *pAudioSink = NULL;
	IMFStreamSink *pStreamSink = NULL;
	IMFMediaTypeHandler *pMediaTypeHandler = NULL;
	IMFMediaType *pMediaType = NULL;
	IMFMediaType *pSinkMediaType = NULL;
	IMFSinkWriter *pSinkWriter = NULL;

	// Set up the source reader for the file.
	CHECK_HR(MFCreateSourceResolver(&pSourceResolver), "MFCreateSourceResolver failed.\n");

	CHECK_HR(pSourceResolver->CreateObjectFromURL(
		L"..\\..\\MediaFiles\\big_buck_bunny.mp4",      // URL of the source.
		MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
		NULL,                       // Optional property store.
		&ObjectType,                // Receives the created object type. 
		&uSource                    // Receives a pointer to the media source.
		), "Failed to create media source resolver for file.\n");

	CHECK_HR(uSource->QueryInterface(IID_PPV_ARGS(&mediaFileSource)),
		"Failed to create media file source.\n");

	CHECK_HR(MFCreateSourceReaderFromMediaSource(mediaFileSource, NULL, &pSourceReader),
		"Error creating media source reader.\n");

	CHECK_HR(pSourceReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pFileAudioMediaType),
		"Error retrieving current media type from first audio stream.\n");

	CHECK_HR(MFCreateMediaType(&pAudioOutType), "Failed to create audio output media type.\n");
	CHECK_HR(pAudioOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio), "Failed to set audio output media major type.\n");
	CHECK_HR(pAudioOutType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float), "Failed to set audio output audio sub type (Float).\n");

	CHECK_HR(pSourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, pAudioOutType),
		"Error setting reader audio output type.\n");

	// Set the sink writer to render the audio (which in this case is the PC speaker).
	CHECK_HR(MFCreateAudioRenderer(NULL, &pAudioSink), "Failed to create audio sink.\n");

	CHECK_HR(pAudioSink->GetStreamSinkByIndex(0, &pStreamSink), "Failed to get audio renderer stream by index.\n");

	CHECK_HR(pStreamSink->GetMediaTypeHandler(&pMediaTypeHandler), "Failed to get media type handler.\n");

	// My speaker has 3 audio types but I was only able to get anywhere with the third one.
	CHECK_HR(pMediaTypeHandler->GetMediaTypeByIndex(2, &pSinkMediaType), "Failed to get sink media type.\n");

	CHECK_HR(pMediaTypeHandler->SetCurrentMediaType(pSinkMediaType), "Failed to set current media type.\n");

	CHECK_HR(MFCreateSinkWriterFromMediaSink(pAudioSink, NULL, &pSinkWriter), "Failed to create sink writer from audio sink.\n");

	getchar();

	// Start the read-write loop.
	printf("Read audio samples from file and write to speaker.\n");

	CHECK_HR(pSinkWriter->BeginWriting(), "Sink writer begin writing call failed.\n");

	IMFSample *audioSample = NULL;
	DWORD streamIndex, flags;
	LONGLONG llAudioTimeStamp, llSampleDuration;
	HRESULT mftProcessInput = S_OK;
	HRESULT mftProcessOutput = S_OK;
	MFT_OUTPUT_STREAM_INFO StreamInfo;
	IMFMediaBuffer *pBuffer = NULL;
	DWORD cbOutBytes = 0;

	while (true)
	{
		CHECK_HR(pSourceReader->ReadSample(
			MF_SOURCE_READER_FIRST_AUDIO_STREAM,
			0,                              // Flags.
			&streamIndex,                   // Receives the actual stream index. 
			&flags,                         // Receives status flags.
			&llAudioTimeStamp,              // Receives the time stamp.
			&audioSample                    // Receives the sample or NULL.
			), "Error reading audio sample.");

		if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
		{
			wprintf(L"\tEnd of stream\n");
			break;
		}
		if (flags & MF_SOURCE_READERF_NEWSTREAM)
		{
			wprintf(L"\tNew stream\n");
			break;
		}
		if (flags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)
		{
			wprintf(L"\tNative type changed\n");
			break;
		}
		if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
		{
			wprintf(L"\tCurrent type changed\n");
			break;
		}
		if (flags & MF_SOURCE_READERF_STREAMTICK)
		{
			printf("Stream tick.\n");
			pSinkWriter->SendStreamTick(0, llAudioTimeStamp);
		}

		if (!audioSample)
		{
			printf("Null audio sample.\n");
		}
		else
		{
			CHECK_HR(audioSample->SetSampleTime(llAudioTimeStamp), "Error setting the audio sample time.\n");

			CHECK_HR(pSinkWriter->WriteSample(0, audioSample), "The stream sink writer was not happy with the sample.\n");
		}
	}

done:

	printf("finished.\n");
	getchar();

	return 0;
}
/// Filename: MFAudioMFT.cpp
///
/// Description:
/// This file contains a C++ console application that is attempting to play the audio stream from a sample MP4 file using the Windows
/// Media Foundation API and with a Resampler IMFTransform applied to the source stream. This sample is an extension of the MFAudio sample and uses the same SourceReader and
/// SinkWriter approach to read and render the stream with the only difference being the MFT tansform applied in the middle.
///
/// Theoretically a different transform could be used such as to encode the audio with a different codec. 
///
/// Note: This sample has a problem with the rendered audio being choppy. The original intention with the MFT was to resample the audio from 22.05KHz to 48KHz to solve the
/// choppiness but it hasn't worked.
///
/// Links:
/// 1. "How to use Resampler MFT": https://code.google.com/p/bitspersampleconv2/wiki/HowToUseResamplerMFT
///
/// History:
/// 24 Feb 2015	Aaron Clauson (aaron@sipsorcery.com)	Created.
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
#include <Wmcodecdsp.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid")

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
	IUnknown *spTransformUnk = NULL;
	IMFTransform *pTransform = NULL; //< this is Resampler MFT
	IWMResamplerProps *spResamplerProps = NULL;
	IMFMediaType *pMFTInputMediaType = NULL, *pMFTOutputMediaType = NULL;
	DWORD mftStatus = 0;
	MFT_OUTPUT_DATA_BUFFER outputDataBuffer;
	DWORD processOutputStatus = 0;
	DWORD sourceStreamIndex = 0;
	GUID streamMajorType;
	BOOL isStreamSelected = true;
	IMFMediaType *pStreamMediaType = NULL;

	// Set up the reader for the file.
	CHECK_HR(MFCreateSourceResolver(&pSourceResolver), "MFCreateSourceResolver failed.\n");

	CHECK_HR(pSourceResolver->CreateObjectFromURL(
		//L"big_buck_bunny_48k.mp4",      // URL of the source.
		L"..\\..\\MediaFiles\\big_buck_bunny.mp4",      // URL of the source.
		//L"max4.mp4",
		MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
		NULL,                       // Optional property store.
		&ObjectType,                // Receives the created object type. 
		&uSource                    // Receives a pointer to the media source.
		), "Failed to create media source resolver for file.\n");

	CHECK_HR(uSource->QueryInterface(IID_PPV_ARGS(&mediaFileSource)),
		"Failed to create media file source.\n");

	CHECK_HR(MFCreateSourceReaderFromMediaSource(mediaFileSource, NULL, &pSourceReader),
		"Error creating media source reader.\n");

	// De-activate the video stream. Doesn't solve the audio chop.
	/*HRESULT streamSelectResult = S_OK;
		
	while (streamSelectResult == S_OK)
	{
		printf("Checking source stream %i.\n", sourceStreamIndex);
		
		streamSelectResult = pSourceReader->GetStreamSelection(sourceStreamIndex, &isStreamSelected);
		
		if (streamSelectResult != S_OK)
		{
			break;
		}
		else
		{
			pSourceReader->GetNativeMediaType(sourceStreamIndex, 0, &pStreamMediaType);
			pStreamMediaType->GetMajorType(&streamMajorType);
		
			if (streamMajorType == MFMediaType_Audio)
			{
				printf("Source stream %i is audio, activating.\n", sourceStreamIndex);
				pSourceReader->SetStreamSelection(sourceStreamIndex, TRUE);
			}
			else if (streamMajorType == MFMediaType_Video)
			{
				printf("Source stream %i is video, deactivating.\n", sourceStreamIndex);
				pSourceReader->SetStreamSelection(sourceStreamIndex, FALSE);
			}
			else
			{
				printf("Source stream %i is not audio or video, deactivating.\n", sourceStreamIndex);
				pSourceReader->SetStreamSelection(sourceStreamIndex, FALSE);
			}
		}
		
		sourceStreamIndex++;
	}*/

	// Select the first audio stream, and deselect all other streams.
	CHECK_HR(pSourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE), "Failed to disable all source reader streams.\n");

	CHECK_HR(pSourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE), "Failed to activate the source reader's first audio stream.\n");

	// Set the audio output type on the source reader.
	CHECK_HR(MFCreateMediaType(&pAudioOutType), "Failed to create audio output media type.\n");
	CHECK_HR(pAudioOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio), "Failed.\n");
	CHECK_HR(pAudioOutType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float), "Failed to set audio output audio sub type (Float).\n");
	CHECK_HR(pAudioOutType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32), "Failed to set audio output bits per sample (32).\n");
	CHECK_HR(pAudioOutType->SetUINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, TRUE), "Failed.\n");
	CHECK_HR(pAudioOutType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2), "Failed.\n");
	CHECK_HR(pAudioOutType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050), "Failed.\n");
	CHECK_HR(pAudioOutType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 8), "Failed.\n");
	CHECK_HR(pAudioOutType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 176400), "Failed.\n");
	CHECK_HR(pAudioOutType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE), "Failed.\n");

	CHECK_HR(pSourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, pAudioOutType),
		"Error setting reader audio output type.\n");

	// printf("Source Reader Output Type:");
	// Dump pAudioOutType.

	CHECK_HR(MFCreateAudioRenderer(NULL, &pAudioSink), "Failed to create audio sink.\n");

	CHECK_HR(pAudioSink->GetStreamSinkByIndex(0, &pStreamSink), "Failed to get audio renderer stream by index.\n");

	CHECK_HR(pStreamSink->GetMediaTypeHandler(&pMediaTypeHandler), "Failed to get media type handler.\n");

	// My speaker has 3 audio types of which I got the furthesr with the third one.
	CHECK_HR(pMediaTypeHandler->GetMediaTypeByIndex(2, &pSinkMediaType), "Failed to get sink media type.\n");

	CHECK_HR(pMediaTypeHandler->SetCurrentMediaType(pSinkMediaType), "Failed to set current media type.\n");

	// printf("Sink Media Type:\n");
	// Dump pSinkMediaType.

	CHECK_HR(MFCreateSinkWriterFromMediaSink(pAudioSink, NULL, &pSinkWriter), "Failed to create sink writer from audio sink.\n");

	// Create resampling MFT.

	CHECK_HR(CoCreateInstance(CLSID_CResamplerMediaObject, NULL, CLSCTX_INPROC_SERVER,
		IID_IUnknown, (void**)&spTransformUnk), "Failed to create resampler MFT.\n");

	CHECK_HR(spTransformUnk->QueryInterface(IID_PPV_ARGS(&pTransform)), "Failed to get IMFTransform interface from resampler MFT object.\n");

	CHECK_HR(spTransformUnk->QueryInterface(IID_PPV_ARGS(&spResamplerProps)), "Failed to get IWMResamplerProps interface from resampler MFT object.\n");

	CHECK_HR(spResamplerProps->SetHalfFilterLength(60), "Failed to set half filter length on resampler properties.\n"); //< best conversion quality

	MFCreateMediaType(&pMFTInputMediaType);
	CHECK_HR(pAudioOutType->CopyAllItems(pMFTInputMediaType), "Failed to copy items from source reader out media type to MFT input type.\n");

	CHECK_HR(pTransform->SetInputType(0, pMFTInputMediaType, 0), "Failed to set input media type on resampler MFT.\n");

	MFCreateMediaType(&pMFTOutputMediaType);
	CHECK_HR(pAudioOutType->CopyAllItems(pMFTOutputMediaType), "Failed to copy items from source reader out media type to MFT output type.\n");

	// Set the media type propeties that the MFT needs to change.
	pMFTOutputMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);	// Speaker value: 48000.
	pMFTOutputMediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 384000); // Speaker value: 384000
	pMFTOutputMediaType->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, 3);	// Speaker value: 3

	CHECK_HR(pTransform->SetOutputType(0, pMFTOutputMediaType, 0), "Failed to set output media type on resampler MFT.\n");

	CHECK_HR(pTransform->GetInputStatus(0, &mftStatus), "Failed to get input status from resampler MFT.\n");
	if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
		printf("E: ApplyTransform() pTransform->GetInputStatus() not accept data.\n");
		goto done;
	}

	CHECK_HR(pTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL), "Failed to process FLUSH command on resampler MFT.\n");
	CHECK_HR(pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), "Failed to process BEGIN_STREAMING command on resampler MFT.\n");
	CHECK_HR(pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), "Failed to process START_OF_STREAM command on resampler MFT.\n");

	memset(&outputDataBuffer, 0, sizeof outputDataBuffer);

	// Ready to go.

	//getchar();

	CHECK_HR(pSinkWriter->BeginWriting(), "Sink writer begin writing call failed.\n");

	printf("Read audio samples from file and write to speaker.\n");

	IMFSample *audioSample = NULL;
	DWORD streamIndex, flags;
	LONGLONG llAudioTimeStamp, llSampleDuration;
	HRESULT mftProcessInput = S_OK;
	HRESULT mftProcessOutput = S_OK;
	MFT_OUTPUT_STREAM_INFO StreamInfo;
	IMFMediaBuffer *pBuffer = NULL;
	DWORD cbOutBytes = 0;
	int sampleCount = 0;

	//while (sampleCount < 10)
	while (true)
	{
		// Initial read results in a null pSample??
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
			CHECK_HR(audioSample->GetSampleDuration(&llSampleDuration), "Error getting audio sample duration.\n");

			// Give the audio sample to the resampler transform.

			CHECK_HR(pTransform->ProcessInput(0, audioSample, 0), "The resampler MFT ProcessInput call failed.\n");

			CHECK_HR(pTransform->GetOutputStreamInfo(0, &StreamInfo), "Failed to get output stream info from resampler MFT.\n");
			if (cbOutBytes < StreamInfo.cbSize) {
				cbOutBytes = StreamInfo.cbSize;
			}

			while (true)
			{
				CHECK_HR(MFCreateSample(&(outputDataBuffer.pSample)), "Failed to create MF sample.\n");
				CHECK_HR(MFCreateMemoryBuffer(cbOutBytes, &pBuffer), "Failed to create memory buffer.\n");
				CHECK_HR(outputDataBuffer.pSample->AddBuffer(pBuffer), "Failed to add sample to buffer.\n");
				outputDataBuffer.dwStreamID = 0;
				outputDataBuffer.dwStatus = 0;
				outputDataBuffer.pEvents = NULL;

				mftProcessOutput = pTransform->ProcessOutput(0, 1, &outputDataBuffer, &processOutputStatus);

				if (mftProcessOutput != MF_E_TRANSFORM_NEED_MORE_INPUT)
				{
					//printf("The resampler MFT ProcessOutput call did not return the expected require more input reponse.\n");
					//break;
					//printf("Resampler output ready.\n");

					CHECK_HR(outputDataBuffer.pSample->SetSampleTime(llAudioTimeStamp), "Error setting MFT sample time.\n");
					CHECK_HR(outputDataBuffer.pSample->SetSampleDuration(llSampleDuration), "Error setting MFT sample duration.\n");

					CHECK_HR(pSinkWriter->WriteSample(0, outputDataBuffer.pSample), "The stream sink writer was not happy with the sample.\n");
				}
				else {
					break;
				}

				pBuffer->Release();
				outputDataBuffer.pSample->Release();
			}
		}

		sampleCount++;
	}

done:

	printf("finished.\n");
	getchar();

	return 0;
}
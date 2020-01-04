/******************************************************************************
* Filename: MFMP4ToYUVWithMFT.cpp
*
* Description:
* This file contains a C++ console application that reads H264 encoded video
* frames from an mp4 file and decodes them to a YUV pixel format and dumps them 
* to an output file stream.
*
* To convert the raw yuv data dumped at the end of this sample use the ffmpeg command below:
* ffmpeg -vcodec rawvideo -s 640x360 -pix_fmt yuv420p -i rawframes.yuv -vframes 1 output.jpeg
* ffmpeg -vcodec rawvideo -s 640x360 -pix_fmt yuv420p -i rawframes.yuv out.avi # (not working)
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 08 Mar 2015	  Aaron Clauson	  Created, Hobart, Australia.
*
* License: Public Domain (no warranty, use at own risk)
/******************************************************************************/

#include "..\Common\MFUtility.h"

#include <stdio.h>
#include <tchar.h>
#include <evr.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wmcodecdsp.h>

#include <fstream>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

#define VIDEO_SAMPLE_WIDTH 640	// Needs to match the video frame width in the input file.
#define VIDEO_SAMPLE_HEIGHT 360 // Needs to match the video frame height in the input file.
#define SAMPLE_COUNT 100
#define SOURCE_FILENAME L"../MediaFiles/big_buck_bunny.mp4"
#define CAPTURE_FILENAME "rawframes.yuv"

int _tmain(int argc, _TCHAR* argv[])
{
	std::ofstream outputBuffer(CAPTURE_FILENAME, std::ios::out | std::ios::binary);

	CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
		"COM initialisation failed.");

	CHECK_HR(MFStartup(MF_VERSION),
		"Media Foundation initialisation failed.");

	IMFSourceResolver *pSourceResolver = NULL;
	IUnknown* uSource = NULL;
	IMFMediaSource *mediaFileSource = NULL;
	IMFAttributes *pVideoReaderAttributes = NULL;
	IMFSourceReader *pSourceReader = NULL;
	MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;
	IMFMediaType *pFileVideoMediaType = NULL;
	IUnknown *spDecTransformUnk = NULL;
	IMFTransform *pDecoderTransform = NULL; // This is H264 Decoder MFT.
	IMFMediaType *pDecInputMediaType = NULL, *pDecOutputMediaType = NULL;
	DWORD mftStatus = 0;

	// Set up the reader for the file.
	CHECK_HR(MFCreateSourceResolver(&pSourceResolver), 
		"MFCreateSourceResolver failed.");

	CHECK_HR(pSourceResolver->CreateObjectFromURL(
		SOURCE_FILENAME,		// URL of the source.
		MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
		NULL,                       // Optional property store.
		&ObjectType,				// Receives the created object type. 
		&uSource					// Receives a pointer to the media source.
		), 
		"Failed to create media source resolver for file.");

	CHECK_HR(uSource->QueryInterface(IID_PPV_ARGS(&mediaFileSource)), 
		"Failed to create media file source.");

	CHECK_HR(MFCreateAttributes(&pVideoReaderAttributes, 2), 
		"Failed to create attributes object for video reader.");

	CHECK_HR(pVideoReaderAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID),
		"Failed to set dev source attribute type for reader config.");

	CHECK_HR(pVideoReaderAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, 1), 
		"Failed to set enable video processing attribute type for reader config.");

	CHECK_HR(MFCreateSourceReaderFromMediaSource(mediaFileSource, pVideoReaderAttributes, &pSourceReader),
		"Error creating media source reader.");

	CHECK_HR(pSourceReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pFileVideoMediaType),
		"Error retrieving current media type from first video stream.");

	// Create H.264 decoder.
	CHECK_HR(CoCreateInstance(CLSID_CMSH264DecoderMFT, NULL, CLSCTX_INPROC_SERVER,
		IID_IUnknown, (void**)&spDecTransformUnk), 
		"Failed to create H264 decoder MFT.");

	CHECK_HR(spDecTransformUnk->QueryInterface(IID_PPV_ARGS(&pDecoderTransform)), "Failed to get IMFTransform interface from H264 decoder MFT object.");

	MFCreateMediaType(&pDecInputMediaType);
	CHECK_HR(pFileVideoMediaType->CopyAllItems(pDecInputMediaType), "Error copying media type attributes to decoder input media type.");
	CHECK_HR(pDecoderTransform->SetInputType(0, pDecInputMediaType, 0), "Failed to set input media type on H.264 decoder MFT.");

	MFCreateMediaType(&pDecOutputMediaType);
	CHECK_HR(pDecOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set major media type.");
	CHECK_HR(pDecOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV), "Failed to set media sub type.");
	CHECK_HR(MFSetAttributeSize(pDecOutputMediaType, MF_MT_FRAME_SIZE, VIDEO_SAMPLE_WIDTH, VIDEO_SAMPLE_HEIGHT), "Failed to set frame size on H264 MFT out type.");
	CHECK_HR(MFSetAttributeRatio(pDecOutputMediaType, MF_MT_FRAME_RATE, 30, 1), "Failed to set frame rate on H264 MFT out type.");
	CHECK_HR(MFSetAttributeRatio(pDecOutputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set aspect ratio on H264 MFT out type.");
	pDecOutputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2);

	CHECK_HR(pDecoderTransform->SetOutputType(0, pDecOutputMediaType, 0), "Failed to set output media type on H.264 decoder MFT.\n");

	CHECK_HR(pDecoderTransform->GetInputStatus(0, &mftStatus), "Failed to get input status from H.264 decoder MFT.\n");
	if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
		printf("H.264 decoder MFT is not accepting data.\n");
		goto done;
	}

	CHECK_HR(pDecoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL), "Failed to process FLUSH command on H.264 decoder MFT.\n");
	CHECK_HR(pDecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), "Failed to process BEGIN_STREAMING command on H.264 decoder MFT.\n");
	CHECK_HR(pDecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), "Failed to process START_OF_STREAM command on H.264 decoder MFT.\n");

	// Ready to start processing frames.

	MFT_OUTPUT_DATA_BUFFER outputDataBuffer;
	DWORD processOutputStatus = 0;
	IMFSample *videoSample = NULL, *reConstructedVideoSample = NULL;
	DWORD streamIndex, flags, bufferCount;
	LONGLONG llVideoTimeStamp = 0, llSampleDuration = 0, yuvVideoTimeStamp = 0, yuvSampleDuration = 0;
	HRESULT mftProcessInput = S_OK;
	HRESULT mftProcessOutput = S_OK;
	MFT_OUTPUT_STREAM_INFO StreamInfo;
	IMFSample *mftOutSample = NULL;
	IMFMediaBuffer *pBuffer = NULL, *reConstructedBuffer = NULL;
	int sampleCount = 0;
	DWORD mftOutFlags;
	//DWORD srcBufLength;
	DWORD sampleFlags = 0;
	LONGLONG reconVideoTimeStamp = 0, reconSampleDuration = 0;
	DWORD reconSampleFlags;

	memset(&outputDataBuffer, 0, sizeof outputDataBuffer);

	while (sampleCount <= SAMPLE_COUNT)
	{
		CHECK_HR(pSourceReader->ReadSample(
			MF_SOURCE_READER_FIRST_VIDEO_STREAM,
			0,                              // Flags.
			&streamIndex,                   // Receives the actual stream index. 
			&flags,                         // Receives status flags.
			&llVideoTimeStamp,              // Receives the time stamp.
			&videoSample                    // Receives the sample or NULL.
			), "Error reading video sample.");

		if (flags & MF_SOURCE_READERF_STREAMTICK)
		{
			printf("Stream tick.\n");
		}

		if (videoSample)
		{
			printf("Processing sample %i.\n", sampleCount);

			CHECK_HR(videoSample->SetSampleTime(llVideoTimeStamp), "Error setting the video sample time.\n");
			CHECK_HR(videoSample->GetSampleDuration(&llSampleDuration), "Error getting video sample duration.\n");
			videoSample->GetSampleFlags(&sampleFlags);
			videoSample->GetBufferCount(&bufferCount);

			printf("Sample time %I64d, sample duration %I64d, sample flags %d, buffer count %i.\n", llVideoTimeStamp, llSampleDuration, sampleFlags, bufferCount);

			// Pass the video sample to the H.264 transform.

			//CHECK_HR(pDecoderTransform->ProcessInput(0, videoSample, 0), "The H264 decoder ProcessInput call failed.\n");

			// Extrtact and then re-construct the sample to simulate processing encoded H264 frames received outside of MF.
			IMFMediaBuffer *srcBuf = NULL;
			DWORD srcBufLength;
			byte *srcByteBuffer;
			DWORD srcBuffCurrLen = 0;
			DWORD srcBuffMaxLen = 0;
			CHECK_HR(videoSample->ConvertToContiguousBuffer(&srcBuf), "ConvertToContiguousBuffer failed.\n");
			CHECK_HR(srcBuf->GetCurrentLength(&srcBufLength), "Get buffer length failed.\n");
			CHECK_HR(srcBuf->Lock(&srcByteBuffer, &srcBuffMaxLen, &srcBuffCurrLen), "Error locking source buffer.\n");
			
			//// Now re-constuct.
			MFCreateSample(&reConstructedVideoSample);
			CHECK_HR(MFCreateMemoryBuffer(srcBufLength, &reConstructedBuffer), "Failed to create memory buffer.\n");
			CHECK_HR(reConstructedVideoSample->AddBuffer(reConstructedBuffer), "Failed to add buffer to re-constructed sample.\n");
			CHECK_HR(reConstructedVideoSample->SetSampleTime(llVideoTimeStamp), "Error setting the recon video sample time.\n");
			CHECK_HR(reConstructedVideoSample->SetSampleDuration(llSampleDuration), "Error setting recon video sample duration.\n");

			byte *reconByteBuffer;
			DWORD reconBuffCurrLen = 0;
			DWORD reconBuffMaxLen = 0;
			CHECK_HR(reConstructedBuffer->Lock(&reconByteBuffer, &reconBuffMaxLen, &reconBuffCurrLen), "Error locking recon buffer.\n");
			memcpy(reconByteBuffer, srcByteBuffer, srcBuffCurrLen);
			CHECK_HR(reConstructedBuffer->Unlock(), "Error unlocking recon buffer.\n");
			reConstructedBuffer->SetCurrentLength(srcBuffCurrLen);

			CHECK_HR(srcBuf->Unlock(), "Error unlocking source buffer.\n");

			CHECK_HR(pDecoderTransform->ProcessInput(0, reConstructedVideoSample, 0), "The H264 decoder ProcessInput call failed.\n");

			CHECK_HR(pDecoderTransform->GetOutputStatus(&mftOutFlags), "H264 MFT GetOutputStatus failed.\n");

			//if (mftOutFlags == MFT_OUTPUT_STATUS_SAMPLE_READY)
			//{
				CHECK_HR(pDecoderTransform->GetOutputStreamInfo(0, &StreamInfo), "Failed to get output stream info from H264 MFT.\n");

				while (true)
				{
					CHECK_HR(MFCreateSample(&mftOutSample), "Failed to create MF sample.\n");
					CHECK_HR(MFCreateMemoryBuffer(StreamInfo.cbSize, &pBuffer), "Failed to create memory buffer.\n");
					CHECK_HR(mftOutSample->AddBuffer(pBuffer), "Failed to add sample to buffer.\n");
					outputDataBuffer.dwStreamID = 0;
					outputDataBuffer.dwStatus = 0;
					outputDataBuffer.pEvents = NULL;
					outputDataBuffer.pSample = mftOutSample;

					mftProcessOutput = pDecoderTransform->ProcessOutput(0, 1, &outputDataBuffer, &processOutputStatus);

					if (mftProcessOutput != MF_E_TRANSFORM_NEED_MORE_INPUT)
					{
						// ToDo: These two lines are not right. Need to work out where to get timestamp and duration from the H264 decoder MFT.
						CHECK_HR(outputDataBuffer.pSample->SetSampleTime(llVideoTimeStamp), "Error getting YUV sample time.\n");
						CHECK_HR(outputDataBuffer.pSample->SetSampleDuration(llSampleDuration), "Error getting YUV sample duration.\n");

						IMFMediaBuffer *buf = NULL;
						DWORD bufLength;
						CHECK_HR(mftOutSample->ConvertToContiguousBuffer(&buf), "ConvertToContiguousBuffer failed.\n");
						CHECK_HR(buf->GetCurrentLength(&bufLength), "Get buffer length failed.\n");

						printf("Writing sample %i, sample time %I64d, sample duration %I64d, sample size %i.\n", sampleCount, yuvVideoTimeStamp, yuvSampleDuration, bufLength);

						byte *byteBuffer;
						DWORD buffCurrLen = 0;
						DWORD buffMaxLen = 0;
						buf->Lock(&byteBuffer, &buffMaxLen, &buffCurrLen);
						outputBuffer.write((char *)byteBuffer, bufLength);
						outputBuffer.flush();
					}
					else {
						break;
					}

					mftOutSample->Release();
				}
			//}
		}

		sampleCount++;
	}

	outputBuffer.close();

done:

	printf("finished.\n");
	auto c = getchar();

	return 0;
}
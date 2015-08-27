/// Filename: MFWebCamToH264Buffer.cpp
///
/// Description:
/// This file contains a C++ console application that captures the realtime video stream from a webcam to an H264 byte array.
/// Rather than using a black box sink writer to do the H264 encoding a media foundation transform is employed. This allows
/// more flexibility about what can be done with the encoded samples. For example they could be packetised in RTP packets
/// and transmitted over a network.
///
/// History:
/// 26 Feb 2015	Aaron Clauson (aaron@sipsorcery.com)	Created.
///
/// License: Public

#include <stdio.h>
#include <tchar.h>
#include <evr.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include "..\Common\MFUtility.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

int _tmain(int argc, _TCHAR* argv[])
{
	const int WEBCAM_DEVICE_INDEX = 1;	// <--- Set to 0 to use default system webcam.
	const WCHAR *CAPTURE_FILENAME = L"sample.mp4";
	const int SAMPLE_COUNT = 100;

	IMFMediaSource *videoSource = NULL;
	UINT32 videoDeviceCount = 0;
	IMFAttributes *videoConfig = NULL;
	IMFActivate **videoDevices = NULL;
	IMFSourceReader *videoReader = NULL;
	WCHAR *webcamFriendlyName;
	IMFMediaType *videoSourceOutputType = NULL, *pSrcOutMediaType = NULL;
	IUnknown *spTransformUnk = NULL;
	IMFTransform *pTransform = NULL; //< this is H264 Encoder MFT
	IWMResamplerProps *spResamplerProps = NULL;
	IMFMediaType *pMFTInputMediaType = NULL, *pMFTOutputMediaType = NULL;
	IMFSinkWriter *pWriter;
	IMFMediaType *pVideoOutType = NULL;
	DWORD writerVideoStreamIndex = 0;
	DWORD totalSampleBufferSize = 0;
	DWORD mftStatus = 0;
	UINT8 blob[] = { 0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xc0, 0x1e, 0x96, 0x54, 0x05, 0x01, 
		0xe9, 0x80, 0x80, 0x40, 0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x3c, 0x80 };

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	MFStartup(MF_VERSION);

	// Get the first available webcam.
	CHECK_HR(MFCreateAttributes(&videoConfig, 1), "Error creating video configuation.\n");

	// Request video capture devices.
	CHECK_HR(videoConfig->SetGUID(
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID), "Error initialising video configuration object.");

	CHECK_HR(MFEnumDeviceSources(videoConfig, &videoDevices, &videoDeviceCount), "Error enumerating video devices.\n");

	CHECK_HR(videoDevices[WEBCAM_DEVICE_INDEX]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &webcamFriendlyName, NULL), "Error retrieving vide device friendly name.\n");

	wprintf(L"First available webcam: %s\n", webcamFriendlyName);

	CHECK_HR(videoDevices[WEBCAM_DEVICE_INDEX]->ActivateObject(IID_PPV_ARGS(&videoSource)), "Error activating video device.\n");

	// Create a source reader.
	CHECK_HR(MFCreateSourceReaderFromMediaSource(
		videoSource,
		videoConfig,
		&videoReader), "Error creating video source reader.\n");

	CHECK_HR(videoReader->GetCurrentMediaType(
		(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
		&videoSourceOutputType), "Error retrieving current media type from first video stream.\n");

	// Note the webcam needs to support this media type. The list of media types supported can be obtained using the ListTypes function in MFUtility.h.
	MFCreateMediaType(&pSrcOutMediaType);
	pSrcOutMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	pSrcOutMediaType->SetGUID(MF_MT_SUBTYPE, WMMEDIASUBTYPE_I420);
	MFSetAttributeSize(pSrcOutMediaType, MF_MT_FRAME_SIZE, 640, 480);

	CHECK_HR(videoReader->SetCurrentMediaType(0, NULL, pSrcOutMediaType), "Failed to set media type on source reader.\n");

	// Create H.264 encoder.
	CHECK_HR(CoCreateInstance(CLSID_CMSH264EncoderMFT, NULL, CLSCTX_INPROC_SERVER,
		IID_IUnknown, (void**)&spTransformUnk), "Failed to create H264 encoder MFT.\n");

	CHECK_HR(spTransformUnk->QueryInterface(IID_PPV_ARGS(&pTransform)), "Failed to get IMFTransform interface from H264 encoder MFT object.\n");

	MFCreateMediaType(&pMFTOutputMediaType);
	pMFTOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	pMFTOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	pMFTOutputMediaType->SetUINT32(MF_MT_AVG_BITRATE, 240000);
	CHECK_HR(MFSetAttributeSize(pMFTOutputMediaType, MF_MT_FRAME_SIZE, 640, 480), "Failed to set frame size on H264 MFT out type.\n");
	CHECK_HR(MFSetAttributeRatio(pMFTOutputMediaType, MF_MT_FRAME_RATE, 30, 1), "Failed to set frame rate on H264 MFT out type.\n");
	CHECK_HR(MFSetAttributeRatio(pMFTOutputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set aspect ratio on H264 MFT out type.\n");
	pMFTOutputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2);
	pMFTOutputMediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

	CHECK_HR(pTransform->SetOutputType(0, pMFTOutputMediaType, 0), "Failed to set output media type on H.264 encoder MFT.\n");

	MFCreateMediaType(&pMFTInputMediaType);
	pMFTInputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	pMFTInputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV);
	CHECK_HR(MFSetAttributeSize(pMFTInputMediaType, MF_MT_FRAME_SIZE, 640, 480), "Failed to set frame size on H264 MFT out type.\n");
	CHECK_HR(MFSetAttributeRatio(pMFTInputMediaType, MF_MT_FRAME_RATE, 30, 1), "Failed to set frame rate on H264 MFT out type.\n");
	CHECK_HR(MFSetAttributeRatio(pMFTInputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set aspect ratio on H264 MFT out type.\n");
	pMFTInputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2);	

	CHECK_HR(pTransform->SetInputType(0, pMFTInputMediaType, 0), "Failed to set input media type on H.264 encoder MFT.\n");

	CHECK_HR(pTransform->GetInputStatus(0, &mftStatus), "Failed to get input status from H.264 MFT.\n");
	if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
		printf("E: ApplyTransform() pTransform->GetInputStatus() not accept data.\n");
		goto done;
	}

	CHECK_HR(pTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL), "Failed to process FLUSH command on H.264 MFT.\n");
	CHECK_HR(pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), "Failed to process BEGIN_STREAMING command on H.264 MFT.\n");
	CHECK_HR(pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), "Failed to process START_OF_STREAM command on H.264 MFT.\n");

	// Create the MP4 sink writer.
	CHECK_HR(MFCreateSinkWriterFromURL(
		CAPTURE_FILENAME,
		NULL,
		NULL,
		&pWriter), "Error creating mp4 sink writer.");

	CHECK_HR(MFTRegisterLocalByCLSID(
		__uuidof(CColorConvertDMO),
		MFT_CATEGORY_VIDEO_PROCESSOR,
		L"",
		MFT_ENUM_FLAG_SYNCMFT,
		0,
		NULL,
		0,
		NULL
		), "Error registering colour converter DSP.\n");

	// Configure the output video type on the sink writer.
	CHECK_HR(MFCreateMediaType(&pVideoOutType), "Configure encoder failed to create media type for video output sink.");
	CHECK_HR(pVideoOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set video writer attribute, media type.");
	CHECK_HR(pVideoOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264), "Failed to set video writer attribute, video format (H.264).");
	CHECK_HR(pVideoOutType->SetUINT32(MF_MT_AVG_BITRATE, 240 * 1000), "Failed to set video writer attribute, bit rate.");
	CHECK_HR(CopyAttribute(videoSourceOutputType, pVideoOutType, MF_MT_FRAME_SIZE), "Failed to set video writer attribute, frame size.");
	CHECK_HR(CopyAttribute(videoSourceOutputType, pVideoOutType, MF_MT_FRAME_RATE), "Failed to set video writer attribute, frame rate.");
	CHECK_HR(CopyAttribute(videoSourceOutputType, pVideoOutType, MF_MT_PIXEL_ASPECT_RATIO), "Failed to set video writer attribute, aspect ratio.");
	CHECK_HR(CopyAttribute(videoSourceOutputType, pVideoOutType, MF_MT_INTERLACE_MODE), "Failed to set video writer attribute, interlace mode.");

	// See http://stackoverflow.com/questions/24411737/media-foundation-imfsinkwriterfinalize-method-fails-under-windows-7-when-mux
	CHECK_HR(pVideoOutType->SetBlob(MF_MT_MPEG_SEQUENCE_HEADER, blob, 24), "Failed to set MF_MT_MPEG_SEQUENCE_HEADER.\n");
	
	CHECK_HR(pWriter->AddStream(pVideoOutType, &writerVideoStreamIndex), "Failed to add the video stream to the sink writer.");
	
	pVideoOutType->Release();

	//CHECK_HR(pWriter->SetInputMediaType(writerVideoStreamIndex, videoSourceOutputType, NULL), "Error setting the sink writer video input type.\n");

	// Ready to go.

	CHECK_HR(pWriter->BeginWriting(), "Sink writer begin writing call failed.\n");

	printf("Reading video samples from webcam.\n");

	MFT_OUTPUT_DATA_BUFFER outputDataBuffer;
	DWORD processOutputStatus = 0;
	IMFSample *videoSample = NULL;
	DWORD streamIndex, flags;
	LONGLONG llVideoTimeStamp, llSampleDuration;
	HRESULT mftProcessInput = S_OK;
	HRESULT mftProcessOutput = S_OK;
	MFT_OUTPUT_STREAM_INFO StreamInfo;
	IMFSample *mftOutSample = NULL;
	IMFMediaBuffer *pBuffer = NULL;
	//DWORD cbOutBytes = 0;
	int sampleCount = 0;
	DWORD mftOutFlags;

	memset(&outputDataBuffer, 0, sizeof outputDataBuffer);

	while (sampleCount <= SAMPLE_COUNT)
	{
		CHECK_HR(videoReader->ReadSample(
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
			pWriter->SendStreamTick(0, llVideoTimeStamp);
		}

		if (videoSample)
		{
			CHECK_HR(videoSample->SetSampleTime(llVideoTimeStamp), "Error setting the video sample time.\n");
			CHECK_HR(videoSample->GetSampleDuration(&llSampleDuration), "Error getting video sample duration.\n");

			// Pass the video sample to the H.264 transform.

			CHECK_HR(pTransform->ProcessInput(0, videoSample, 0), "The resampler H264 ProcessInput call failed.\n");

			CHECK_HR(pTransform->GetOutputStatus(&mftOutFlags), "H264 MFT GetOutputStatus failed.\n");

			if (mftOutFlags == MFT_OUTPUT_STATUS_SAMPLE_READY)
			{
				CHECK_HR(pTransform->GetOutputStreamInfo(0, &StreamInfo), "Failed to get output stream info from H264 MFT.\n");

				while (true)
				{
					CHECK_HR(MFCreateSample(&mftOutSample), "Failed to create MF sample.\n");
					CHECK_HR(MFCreateMemoryBuffer(StreamInfo.cbSize, &pBuffer), "Failed to create memory buffer.\n");
					CHECK_HR(mftOutSample->AddBuffer(pBuffer), "Failed to add sample to buffer.\n");
					outputDataBuffer.dwStreamID = 0;
					outputDataBuffer.dwStatus = 0;
					outputDataBuffer.pEvents = NULL;
					outputDataBuffer.pSample = mftOutSample;

					mftProcessOutput = pTransform->ProcessOutput(0, 1, &outputDataBuffer, &processOutputStatus);

					if (mftProcessOutput != MF_E_TRANSFORM_NEED_MORE_INPUT)
					{
						CHECK_HR(outputDataBuffer.pSample->SetSampleTime(llVideoTimeStamp), "Error setting MFT sample time.\n");
						CHECK_HR(outputDataBuffer.pSample->SetSampleDuration(llSampleDuration), "Error setting MFT sample duration.\n");
						
						IMFMediaBuffer *buf = NULL;
						DWORD bufLength;
						CHECK_HR(mftOutSample->ConvertToContiguousBuffer(&buf), "ConvertToContiguousBuffer failed.\n");
						CHECK_HR(buf->GetCurrentLength(&bufLength), "Get buffer length failed.\n");

						totalSampleBufferSize += bufLength;

						printf("Writing sample %i, sample time %I64d, sample duration %I64d, sample size %i.\n", sampleCount, llVideoTimeStamp, llSampleDuration, bufLength);
						CHECK_HR(pWriter->WriteSample(writerVideoStreamIndex, outputDataBuffer.pSample), "The stream sink writer was not happy with the sample.\n");
					}
					else {
						break;
					}

					pBuffer->Release();
					mftOutSample->Release();
				}
			}

			SafeRelease(&videoSample);
		}

		sampleCount++;
	}

	printf("Total sample buffer size %i.\n", totalSampleBufferSize);
	printf("Finalising the capture.\n");

	if (pWriter)
	{
		// See http://stackoverflow.com/questions/24411737/media-foundation-imfsinkwriterfinalize-method-fails-under-windows-7-when-mux for why the Finalize call can fail with MF_E_ATTRIBUTENOTFOUND .
		CHECK_HR(pWriter->Finalize(), "Error finalising H.264 sink writer.\n");
	}

done:

	printf("finished.\n");
	getchar();

	return 0;
}
/// Filename: MFWebCamRtp.cpp
///
/// Description:
/// This file contains a C++ console application that captures the realtime video stream from a webcam using 
/// Windows Media Foundation, encodes it as H264 and then transmits it to an RTP end point using the real-time
/// communications API from Live555 (http://live555.com/).
///
/// To view the RTP feed produced by this sample the steps are:
/// 1. Download ffplay from http://ffmpeg.zeranoe.com/builds/ (the static build has a ready to go ffplay executable),
/// 2. Create a file called test.sdp with contents as below:
/// v=0
/// o = -0 0 IN IP4 127.0.0.1
/// s = No Name
/// t = 0 0
/// c = IN IP4 127.0.0.1
/// m = video 1234 RTP / AVP 96
/// a = rtpmap:96 H264 / 90000
/// a = fmtp : 96 packetization - mode = 1
/// 3. Start ffplay BEFORE running this sample:
/// ffplay -i test.sdp -x 800 -y 600 -profile:v baseline
///
/// History:
/// 07 Sep 2015	Aaron Clauson (aaron@sipsorcery.com)	Created.
///
/// License: Public
/// License for Live555: LGPL (http://live555.com/liveMedia/#license)

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"

#include <stdint.h>
#include <stdio.h>
#include <tchar.h>
#include <evr.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include <codecapi.h>
#include "..\Common\MFUtility.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

class MediaFoundationH264LiveSource :
	public FramedSource
{
private:
	static const int CAMERA_RESOLUTION_WIDTH = 640; // 800; // 1280;
	static const int CAMERA_RESOLUTION_HEIGHT = 480; // 600; //  1024;
	static const int TARGET_FRAME_RATE = 30;// 5; 15; 30	// Note that this if the video device does not support this frame rate the video source reader will fail to initialise.
	static const int TARGET_AVERAGE_BIT_RATE = 1000000; // Adjusting this affects the quality of the H264 bit stream.
	static const int WEBCAM_DEVICE_INDEX = 1;	// <--- Set to 0 to use default system webcam.

	bool _isInitialised = false;
	EventTriggerId eventTriggerId = 0;
	int _frameCount = 0;
	long int _lastSendAt;

	IMFTransform *_pTransform = NULL; //< this is H264 Encoder MFT
	IMFSourceReader *_videoReader = NULL;
	MFT_OUTPUT_DATA_BUFFER _outputDataBuffer;
	
	IMFMediaSource *videoSource = NULL;
	IMFMediaType *videoSourceOutputType = NULL, *pSrcOutMediaType = NULL;
	IUnknown *spTransformUnk = NULL;
	IMFMediaType *pMFTInputMediaType = NULL, *pMFTOutputMediaType = NULL;
	DWORD mftStatus = 0;

public:
	static MediaFoundationH264LiveSource* createNew(UsageEnvironment& env)
	{
		return new MediaFoundationH264LiveSource(env);
	}

	MediaFoundationH264LiveSource(UsageEnvironment& env) :
		FramedSource(env)
	{
		_lastSendAt = GetTickCount();
		eventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);
	}

	~MediaFoundationH264LiveSource()
	{ }

	bool isH264VideoStreamFramer() const {
		return true;
	}

	static void deliverFrame0(void* clientData) {
		((MediaFoundationH264LiveSource*)clientData)->doGetNextFrame();
	}

	bool initialise()
	{
		UINT32 videoDeviceCount = 0;
		IMFAttributes *videoConfig = NULL;
		IMFActivate **videoDevices = NULL;
		WCHAR *webcamFriendlyName;
		
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
			&_videoReader), "Error creating video source reader.\n");

		//ListModes(_videoReader);

		/*CHECK_HR(_videoReader->GetCurrentMediaType(
			(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
			&videoSourceOutputType), "Error retrieving current media type from first video stream.\n");

		Console::WriteLine(GetMediaTypeDescription(videoSourceOutputType));*/

		// Note the webcam needs to support this media type. The list of media types supported can be obtained using the ListTypes function in MFUtility.h.
		MFCreateMediaType(&pSrcOutMediaType);
		pSrcOutMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		pSrcOutMediaType->SetGUID(MF_MT_SUBTYPE, WMMEDIASUBTYPE_I420);
		//pSrcOutMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24);
		MFSetAttributeSize(pSrcOutMediaType, MF_MT_FRAME_SIZE, CAMERA_RESOLUTION_WIDTH, CAMERA_RESOLUTION_HEIGHT);
		CHECK_HR(MFSetAttributeRatio(pSrcOutMediaType, MF_MT_FRAME_RATE, TARGET_FRAME_RATE, 1), "Failed to set frame rate on video device out type.\n");

		CHECK_HR(_videoReader->SetCurrentMediaType(0, NULL, pSrcOutMediaType), "Failed to set media type on source reader.\n");
		//CHECK_HR(_videoReader->SetCurrentMediaType(0, NULL, videoSourceOutputType), "Failed to setdefault  media type on source reader.\n");

		// Create H.264 encoder.
		CHECK_HR(CoCreateInstance(CLSID_CMSH264EncoderMFT, NULL, CLSCTX_INPROC_SERVER,
			IID_IUnknown, (void**)&spTransformUnk), "Failed to create H264 encoder MFT.\n");

		CHECK_HR(spTransformUnk->QueryInterface(IID_PPV_ARGS(&_pTransform)), "Failed to get IMFTransform interface from H264 encoder MFT object.\n");

		MFCreateMediaType(&pMFTOutputMediaType);
		pMFTOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		pMFTOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
		//pMFTOutputMediaType->SetUINT32(MF_MT_AVG_BITRATE, 240000);
		CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_MT_AVG_BITRATE, TARGET_AVERAGE_BIT_RATE), "Failed to set average bit rate on H264 output media type.\n");
		CHECK_HR(MFSetAttributeSize(pMFTOutputMediaType, MF_MT_FRAME_SIZE, CAMERA_RESOLUTION_WIDTH, CAMERA_RESOLUTION_HEIGHT), "Failed to set frame size on H264 MFT out type.\n");
		CHECK_HR(MFSetAttributeRatio(pMFTOutputMediaType, MF_MT_FRAME_RATE, TARGET_FRAME_RATE, 1), "Failed to set frame rate on H264 MFT out type.\n");
		CHECK_HR(MFSetAttributeRatio(pMFTOutputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set aspect ratio on H264 MFT out type.\n");
		pMFTOutputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2);	// 2 = Progressive scan, i.e. non-interlaced.
		pMFTOutputMediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
		//CHECK_HR(MFSetAttributeRatio(pMFTOutputMediaType, MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Base), "Failed to set profile on H264 MFT out type.\n");
		//CHECK_HR(pMFTOutputMediaType->SetDouble(MF_MT_MPEG2_LEVEL, 3.1), "Failed to set level on H264 MFT out type.\n");
		//CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_MT_MAX_KEYFRAME_SPACING, 10), "Failed to set key frame interval on H264 MFT out type.\n");
		//CHECK_HR(pMFTOutputMediaType->SetUINT32(CODECAPI_AVEncCommonQuality, 100), "Failed to set H264 codec qulaity.\n");
		//hr = pAttributes->SetUINT32(CODECAPI_AVEncMPVGOPSize, 1)

		CHECK_HR(_pTransform->SetOutputType(0, pMFTOutputMediaType, 0), "Failed to set output media type on H.264 encoder MFT.\n");

		MFCreateMediaType(&pMFTInputMediaType);
		pMFTInputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		pMFTInputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV);
		CHECK_HR(MFSetAttributeSize(pMFTInputMediaType, MF_MT_FRAME_SIZE, CAMERA_RESOLUTION_WIDTH, CAMERA_RESOLUTION_HEIGHT), "Failed to set frame size on H264 MFT out type.\n");
		CHECK_HR(MFSetAttributeRatio(pMFTInputMediaType, MF_MT_FRAME_RATE, TARGET_FRAME_RATE, 1), "Failed to set frame rate on H264 MFT out type.\n");
		CHECK_HR(MFSetAttributeRatio(pMFTInputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set aspect ratio on H264 MFT out type.\n");
		pMFTInputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2);

		CHECK_HR(_pTransform->SetInputType(0, pMFTInputMediaType, 0), "Failed to set input media type on H.264 encoder MFT.\n");

		CHECK_HR(_pTransform->GetInputStatus(0, &mftStatus), "Failed to get input status from H.264 MFT.\n");
		if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
			printf("E: ApplyTransform() pTransform->GetInputStatus() not accept data.\n");
			goto done;
		}

		//Console::WriteLine(GetMediaTypeDescription(pMFTOutputMediaType));

		CHECK_HR(_pTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL), "Failed to process FLUSH command on H.264 MFT.\n");
		CHECK_HR(_pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), "Failed to process BEGIN_STREAMING command on H.264 MFT.\n");
		CHECK_HR(_pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), "Failed to process START_OF_STREAM command on H.264 MFT.\n");

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

		memset(&_outputDataBuffer, 0, sizeof _outputDataBuffer);

		return true;

	done:

		printf("MediaFoundationH264LiveSource initialisation failed.\n");
		return false;
	}

	virtual void doGetNextFrame()
	{
		if (!_isInitialised)
		{
			_isInitialised = true;
			if (!initialise())
			{
				printf("Video device initialisation failed, stopping.");
				return;
			}
		}

		if (!isCurrentlyAwaitingData()) return;

		DWORD processOutputStatus = 0;
		IMFSample *videoSample = NULL;
		DWORD streamIndex, flags;
		LONGLONG llVideoTimeStamp, llSampleDuration;
		HRESULT mftProcessInput = S_OK;
		HRESULT mftProcessOutput = S_OK;
		MFT_OUTPUT_STREAM_INFO StreamInfo;
		IMFMediaBuffer *pBuffer = NULL;
		IMFSample *mftOutSample = NULL;
		DWORD mftOutFlags;
		bool frameSent = false;

		CHECK_HR(_videoReader->ReadSample(
			MF_SOURCE_READER_FIRST_VIDEO_STREAM,
			0,                              // Flags.
			&streamIndex,                   // Receives the actual stream index. 
			&flags,                         // Receives status flags.
			&llVideoTimeStamp,              // Receives the time stamp.
			&videoSample                    // Receives the sample or NULL.
			), "Error reading video sample.");

		if (videoSample)
		{
			_frameCount++;

			CHECK_HR(videoSample->SetSampleTime(llVideoTimeStamp), "Error setting the video sample time.\n");
			CHECK_HR(videoSample->GetSampleDuration(&llSampleDuration), "Error getting video sample duration.\n");

			// Pass the video sample to the H.264 transform.

			CHECK_HR(_pTransform->ProcessInput(0, videoSample, 0), "The resampler H264 ProcessInput call failed.\n");

			CHECK_HR(_pTransform->GetOutputStatus(&mftOutFlags), "H264 MFT GetOutputStatus failed.\n");

			if (mftOutFlags == MFT_OUTPUT_STATUS_SAMPLE_READY)
			{
				printf("Sample ready.\n");

				CHECK_HR(_pTransform->GetOutputStreamInfo(0, &StreamInfo), "Failed to get output stream info from H264 MFT.\n");

				CHECK_HR(MFCreateSample(&mftOutSample), "Failed to create MF sample.\n");
				CHECK_HR(MFCreateMemoryBuffer(StreamInfo.cbSize, &pBuffer), "Failed to create memory buffer.\n");
				CHECK_HR(mftOutSample->AddBuffer(pBuffer), "Failed to add sample to buffer.\n");

				while (true)
				{
					_outputDataBuffer.dwStreamID = 0;
					_outputDataBuffer.dwStatus = 0;
					_outputDataBuffer.pEvents = NULL;
					_outputDataBuffer.pSample = mftOutSample;

					mftProcessOutput = _pTransform->ProcessOutput(0, 1, &_outputDataBuffer, &processOutputStatus);

					if (mftProcessOutput != MF_E_TRANSFORM_NEED_MORE_INPUT)
					{
						CHECK_HR(_outputDataBuffer.pSample->SetSampleTime(llVideoTimeStamp), "Error setting MFT sample time.\n");
						CHECK_HR(_outputDataBuffer.pSample->SetSampleDuration(llSampleDuration), "Error setting MFT sample duration.\n");

						IMFMediaBuffer *buf = NULL;
						DWORD bufLength;
						CHECK_HR(_outputDataBuffer.pSample->ConvertToContiguousBuffer(&buf), "ConvertToContiguousBuffer failed.\n");
						CHECK_HR(buf->GetCurrentLength(&bufLength), "Get buffer length failed.\n");
						BYTE * rawBuffer = NULL;

						auto now = GetTickCount();

						printf("Writing sample %i, spacing %I64dms, sample time %I64d, sample duration %I64d, sample size %i.\n", _frameCount, now - _lastSendAt, llVideoTimeStamp, llSampleDuration, bufLength);

						fFrameSize = bufLength;
						fDurationInMicroseconds = 0;
						gettimeofday(&fPresentationTime, NULL);

						buf->Lock(&rawBuffer, NULL, NULL);
						memmove(fTo, rawBuffer, fFrameSize);

						FramedSource::afterGetting(this);

						buf->Unlock();
						SafeRelease(&buf);

						frameSent = true;
						_lastSendAt = GetTickCount();
					}

					SafeRelease(&pBuffer);
					SafeRelease(&mftOutSample);

					break;
				}
			}
			else {
				printf("No sample.\n");
			}

			SafeRelease(&videoSample);
		}

		if (!frameSent)
		{
			envir().taskScheduler().triggerEvent(eventTriggerId, this);
		}

		return;

	done:

		printf("MediaFoundationH264LiveSource doGetNextFrame failed.\n");
	}
};

int main()
{
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	MFStartup(MF_VERSION);

	TaskScheduler* scheduler = BasicTaskScheduler::createNew();
	UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);

	in_addr dstAddr = { 127, 0, 0, 1 };
	Groupsock rtpGroupsock(*env, dstAddr, 1233, 255);
	rtpGroupsock.addDestination(dstAddr, 1234, 0);
	RTPSink * rtpSink = H264VideoRTPSink::createNew(*env, &rtpGroupsock, 96);

	MediaFoundationH264LiveSource * mediaFoundationH264Source = MediaFoundationH264LiveSource::createNew(*env);
	rtpSink->startPlaying(*mediaFoundationH264Source, NULL, NULL);

	// This function call does not return.
	env->taskScheduler().doEventLoop();

	return 0;
}
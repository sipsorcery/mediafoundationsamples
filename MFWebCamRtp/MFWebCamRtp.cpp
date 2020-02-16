/******************************************************************************
* Filename: MFWebCamRtp.cpp
*
* Description:
* This file contains a C++ console application that captures the real-time video
* stream from a webcam using Windows Media Foundation, encodes it as H264 and then
* transmits it to an RTP end point.
*
* To view the RTP feed produced by this sample the steps are:
* 1. Download ffplay from http://ffmpeg.zeranoe.com/builds/ (the static build has
*    a ready to go ffplay executable),
*
* 2. Create a file called test.sdp with contents as below:
* v=0
* o=-0 0 IN IP4 127.0.0.1
* s=No Name
* t=0 0
* c=IN IP4 127.0.0.1
* m=video 1234 RTP/AVP 96
* a=rtpmap:96 H264/90000
* a=fmtp:96 packetization-mode=1
*
* 3. Start ffplay BEFORE running this sample:
* ffplay -i test.sdp -x 640 -y 480 -profile:v baseline -protocol_whitelist "file,rtp,udp"
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 07 Sep 2015	Aaron Clauson	Created, Hobart, Australia.
* 04 Jan 2020	Aaron Clauson	Removed live555 (sledgehammer for a nail for this sample).
* 10 Jan 2020 Aaron Clauson   Added rudimentary RTP packetisation (suitable for proof of concept only).
*
* License: Public Domain (no warranty, use at own risk)
/******************************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "../Common/MFUtility.h"

#include <stdio.h>
#include <tchar.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include <codecapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")
#pragma comment(lib, "Ws2_32.lib")

#define WEBCAM_DEVICE_INDEX 0	    // Adjust according to desired video capture device.
#define OUTPUT_FRAME_WIDTH 640		// Adjust if the webcam does not support this frame width.
#define OUTPUT_FRAME_HEIGHT 480		// Adjust if the webcam does not support this frame height.
#define OUTPUT_FRAME_RATE 30      // Adjust if the webcam does not support this frame rate.
#define RTP_MAX_PAYLOAD 1400      // Maximum size of an RTP packet, needs to be under the Ethernet MTU.
#define RTP_HEADER_LENGTH 12
#define RTP_VERSION 2
#define RTP_PAYLOAD_ID 96         // Needs to match the attribute set in the SDP (a=rtpmap:96 H264/90000).
#define H264_RTP_HEADER_LENGTH 2
#define FFPLAY_RTP_PORT 1234      // The port this sample will send to.

/**
* Minimal 12 byte RTP header structure. No facility for extensions etc.
*/
class RtpHeader
{
public:
  uint8_t Version = RTP_VERSION;   // 2 bits.
  uint8_t PaddingFlag  = 0;        // 1 bit.
  uint8_t HeaderExtensionFlag = 0; // 1 bit.
  uint8_t CSRCCount = 0;           // 4 bits.
  uint8_t MarkerBit = 0;           // 1 bit.
  uint8_t PayloadType = 0;         // 7 bits.
  uint16_t SeqNum = 0;             // 16 bits.
  uint32_t Timestamp = 0;          // 32 bits.
  uint32_t SyncSource = 0;         // 32 bits.

  void Serialise(uint8_t** buf)
  {
    *buf = (uint8_t*)calloc(RTP_HEADER_LENGTH, 1);
    *(*buf) = (Version << 6 & 0xC0) | (PaddingFlag << 5 & 0x20) | (HeaderExtensionFlag << 4 & 0x10) | (CSRCCount & 0x0f);
    *(*buf + 1) = (MarkerBit << 7 & 0x80) | (PayloadType & 0x7f);
    *(*buf + 2) = SeqNum >> 8 & 0xff;
    *(*buf + 3) = SeqNum & 0xff;
    *(*buf + 4) = Timestamp >> 24 & 0xff;
    *(*buf + 5) = Timestamp >> 16 & 0xff;
    *(*buf + 6) = Timestamp >> 8 & 0xff;
    *(*buf + 7) = Timestamp & 0xff;
    *(*buf + 8) = SyncSource >> 24 & 0xff;
    *(*buf + 9) = SyncSource >> 16 & 0xff;
    *(*buf + 10) = SyncSource >> 8 & 0xff;
    *(*buf + 11) = SyncSource & 0xff;
  }
};

// Forward function definitions.
HRESULT SendH264RtpSample(SOCKET socket, sockaddr_in & dst, IMFSample* pH264Sample, uint32_t ssrc, uint32_t timestamp, uint16_t* seqNum);

int main()
{
  IMFMediaSource* pVideoSource = NULL;
  IMFSourceReader* pVideoReader = NULL;
  WCHAR* webcamFriendlyName;
  IMFMediaType* pSrcOutMediaType = NULL;
  IUnknown* spEncoderTransfromUnk = NULL;
  IMFTransform* pEncoderTransfrom = NULL; // This is H264 Encoder MFT.
  IMFMediaType* pMFTInputMediaType = NULL, * pMFTOutputMediaType = NULL;
  UINT friendlyNameLength = 0;
  IUnknown* spDecTransformUnk = NULL;
  IMFTransform* pDecoderTransform = NULL; // This is H264 Decoder MFT.
  IMFMediaType* pDecInputMediaType = NULL, * pDecOutputMediaType = NULL;
  DWORD mftStatus = 0;

  WSADATA wsaData;
  uint16_t rtpSsrc = 3334; // Supposed to be pseudo-random.
  uint16_t rtpSeqNum = 0;
  uint32_t rtpTimestamp = 0;
  SOCKET rtpSocket = INVALID_SOCKET;
  sockaddr_in service, dest;

  CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
    "COM initialisation failed.");

  CHECK_HR(MFStartup(MF_VERSION),
    "Media Foundation initialisation failed.");

  // Initialize Winsock
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    printf("WSAStartup failed: %d\n", iResult);
    return 1;
  }

  rtpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (rtpSocket == INVALID_SOCKET) {
    wprintf(L"socket function failed with error: %u\n", WSAGetLastError());
    WSACleanup();
    return 1;
  }

  //----------------------
    // The sockaddr_in structure specifies the address family,
    // IP address, and port for the socket that is being bound.
  service.sin_family = AF_INET;
  service.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  service.sin_port = 0; // htons(27015);

  //----------------------
  // Bind the socket.
  iResult = bind(rtpSocket, (SOCKADDR*)&service, sizeof (service));
  if (iResult == SOCKET_ERROR) {
    wprintf(L"bind failed with error %u\n", WSAGetLastError());
    closesocket(rtpSocket);
    WSACleanup();
    return 1;
  }
  else {
    wprintf(L"bind returned success\n");
  }

  dest.sin_family = AF_INET;
  inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr.s_addr);
  dest.sin_port = htons(FFPLAY_RTP_PORT);

  // Get video capture device.
  CHECK_HR(GetVideoSourceFromDevice(WEBCAM_DEVICE_INDEX, &pVideoSource, &pVideoReader),
    "Failed to get webcam video source.");

  // Note the webcam needs to support this media type. 
  MFCreateMediaType(&pSrcOutMediaType);
  CHECK_HR(pSrcOutMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set major video type.");
  CHECK_HR(pSrcOutMediaType->SetGUID(MF_MT_SUBTYPE, WMMEDIASUBTYPE_I420), "Failed to set video sub type to I420.");
  //CHECK_HR(pSrcOutMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24), "Failed to set video sub type.");
  CHECK_HR(MFSetAttributeRatio(pSrcOutMediaType, MF_MT_FRAME_RATE, OUTPUT_FRAME_RATE, 1), "Failed to set frame rate on source reader out type.");
  CHECK_HR(MFSetAttributeSize(pSrcOutMediaType, MF_MT_FRAME_SIZE, OUTPUT_FRAME_WIDTH, OUTPUT_FRAME_HEIGHT), "Failed to set frame size.");

  CHECK_HR(pVideoReader->SetCurrentMediaType(0, NULL, pSrcOutMediaType),
    "Failed to set media type on source reader.");

  printf("%s\n", GetMediaTypeDescription(pSrcOutMediaType).c_str());

  // Create H.264 encoder.
  CHECK_HR(CoCreateInstance(CLSID_CMSH264EncoderMFT, NULL, CLSCTX_INPROC_SERVER,
    IID_IUnknown, (void**)&spEncoderTransfromUnk),
    "Failed to create H264 encoder MFT.");

  CHECK_HR(spEncoderTransfromUnk->QueryInterface(IID_PPV_ARGS(&pEncoderTransfrom)),
    "Failed to get IMFTransform interface from H264 encoder MFT object.");

  MFCreateMediaType(&pMFTInputMediaType);
  CHECK_HR(pSrcOutMediaType->CopyAllItems(pMFTInputMediaType), "Error copying media type attributes to decoder output media type.");
  CHECK_HR(pMFTInputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV), "Error setting video subtype.");

  MFCreateMediaType(&pMFTOutputMediaType);
  CHECK_HR(pMFTInputMediaType->CopyAllItems(pMFTOutputMediaType), "Error copying media type attributes from mft input type to mft output type.");
  CHECK_HR(pMFTOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264), "Error setting video sub type.");
  CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_MT_AVG_BITRATE, 240000), "Error setting average bit rate.");
  CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2), "Error setting interlace mode.");
  //CHECK_HR(MFSetAttributeRatio(pMFTOutputMediaType, MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Base, 1), "Failed to set profile on H264 MFT out type.");
  CHECK_HR(MFSetAttributeRatio(pMFTOutputMediaType, MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_ConstrainedBase, 1), "Failed to set profile on H264 MFT out type.");
  //CHECK_HR(pMFTOutputMediaType->SetDouble(MF_MT_MPEG2_LEVEL, 3.1), "Failed to set level on H264 MFT out type.\n");
  //CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_MT_MAX_KEYFRAME_SPACING, 10), "Failed to set key frame interval on H264 MFT out type.\n");
  //CHECK_HR(pMFTOutputMediaType->SetUINT32(CODECAPI_AVEncCommonQuality, 100), "Failed to set H264 codec quality.\n");

  std::cout << "H264 encoder output type: " << GetMediaTypeDescription(pMFTOutputMediaType) << std::endl;

  CHECK_HR(pEncoderTransfrom->SetOutputType(0, pMFTOutputMediaType, 0),
    "Failed to set output media type on H.264 encoder MFT.");

  std::cout << "H264 encoder input type: " << GetMediaTypeDescription(pMFTInputMediaType) << std::endl;

  CHECK_HR(pEncoderTransfrom->SetInputType(0, pMFTInputMediaType, 0),
    "Failed to set input media type on H.264 encoder MFT.");

  CHECK_HR(pEncoderTransfrom->GetInputStatus(0, &mftStatus), "Failed to get input status from H.264 MFT.");
  if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
    printf("E: ApplyTransform() pEncoderTransfrom->GetInputStatus() not accept data.\n");
    goto done;
  }

  //CHECK_HR(pEncoderTransfrom->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL), "Failed to process FLUSH command on H.264 MFT.");
  CHECK_HR(pEncoderTransfrom->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), "Failed to process BEGIN_STREAMING command on H.264 MFT.");
  CHECK_HR(pEncoderTransfrom->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), "Failed to process START_OF_STREAM command on H.264 MFT.");

  // Ready to go.

  printf("Reading video samples from webcam.\n");

  IMFSample* pVideoSample = NULL, * pH264EncodeOutSample = NULL;
  DWORD streamIndex = 0, flags = 0, sampleFlags = 0;
  LONGLONG llVideoTimeStamp, llSampleDuration;
  int sampleCount = 0;
  BOOL h264EncodeTransformFlushed = FALSE;

  while (true)
  {
    CHECK_HR(pVideoReader->ReadSample(
      MF_SOURCE_READER_FIRST_VIDEO_STREAM,
      0,                              // Flags.
      &streamIndex,                   // Receives the actual stream index. 
      &flags,                         // Receives status flags.
      &llVideoTimeStamp,              // Receives the time stamp.
      &pVideoSample                   // Receives the sample or NULL.
    ), "Error reading video sample.");

    if (flags & MF_SOURCE_READERF_STREAMTICK)
    {
      printf("\tStream tick.\n");
    }
    if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
    {
      printf("\tEnd of stream.\n");
      break;
    }
    if (flags & MF_SOURCE_READERF_NEWSTREAM)
    {
      printf("\tNew stream.\n");
      break;
    }
    if (flags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)
    {
      printf("\tNative type changed.\n");
      break;
    }
    if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
    {
      printf("\tCurrent type changed.\n");
      break;
    }

    if (pVideoSample)
    {
      CHECK_HR(pVideoSample->SetSampleTime(llVideoTimeStamp), "Error setting the video sample time.");
      CHECK_HR(pVideoSample->GetSampleDuration(&llSampleDuration), "Error getting video sample duration.");
      CHECK_HR(pVideoSample->GetSampleFlags(&sampleFlags), "Error getting sample flags.");

      //printf("Sample count %d, Sample flags %d, sample duration %I64d, sample time %I64d\n", sampleCount, sampleFlags, llSampleDuration, llVideoTimeStamp);

      // Apply the H264 encoder transform
      CHECK_HR(pEncoderTransfrom->ProcessInput(0, pVideoSample, 0),
        "The H264 encoder ProcessInput call failed.");

      // ***** H264 ENcoder transform processing loop. *****

      HRESULT getEncoderResult = S_OK;
      while (getEncoderResult == S_OK) {

        getEncoderResult = GetTransformOutput(pEncoderTransfrom, &pH264EncodeOutSample, &h264EncodeTransformFlushed);

        if (getEncoderResult != S_OK && getEncoderResult != MF_E_TRANSFORM_NEED_MORE_INPUT) {
          printf("Error getting H264 encoder transform output, error code %.2X.\n", getEncoderResult);
          goto done;
        }

        if (h264EncodeTransformFlushed == TRUE) {
          // H264 encoder format changed. Clear the capture file and start again.
          printf("H264 encoder transform flushed stream.\n");
        }
        else if (pH264EncodeOutSample != NULL) {

          //printf("H264 sample ready for transmission.\n");

          SendH264RtpSample(rtpSocket, dest, pH264EncodeOutSample, rtpSsrc, (uint32_t)(llVideoTimeStamp / 10000), &rtpSeqNum);
        }

        SAFE_RELEASE(pH264EncodeOutSample);
      }
      // *****

      sampleCount++;

      // Note: Apart from memory leak issues if the media samples are not released the videoReader->ReadSample
      // blocks when it is unable to allocate a new sample.
      SAFE_RELEASE(pVideoSample);
      SAFE_RELEASE(pH264EncodeOutSample);
    }
  }

done:

  printf("finished.\n");
  auto c = getchar();

  SAFE_RELEASE(pVideoSource);
  SAFE_RELEASE(pVideoReader);
  SAFE_RELEASE(pSrcOutMediaType);
  SAFE_RELEASE(spEncoderTransfromUnk);
  SAFE_RELEASE(pEncoderTransfrom);
  SAFE_RELEASE(pMFTInputMediaType);
  SAFE_RELEASE(pMFTOutputMediaType);

  WSACleanup();

  return 0;
}

HRESULT SendH264RtpSample(SOCKET socket, sockaddr_in& dst, IMFSample* pH264Sample, uint32_t ssrc, uint32_t timestamp, uint16_t* seqNum)
{
  static uint16_t h264HeaderStart = 0x1c89;   // Start RTP packet in frame 0x1c 0x89
  static uint16_t h264HeaderMiddle = 0x1c09;  // Middle RTP packet in frame 0x1c 0x09
  static uint16_t h264HeaderEnd = 0x1c49;     // Last RTP packet in frame 0x1c 0x49

  HRESULT hr = S_OK;

  IMFMediaBuffer* buf = NULL;
  DWORD frameLength = 0, buffCurrLen = 0, buffMaxLen = 0;
  byte* frameData = NULL;

  hr = pH264Sample->ConvertToContiguousBuffer(&buf);
  CHECK_HR(hr, "ConvertToContiguousBuffer failed.");

  hr = buf->GetCurrentLength(&frameLength);
  CHECK_HR(hr, "Get buffer length failed.");

  hr = buf->Lock(&frameData, &buffMaxLen, &buffCurrLen);
  CHECK_HR(hr, "Failed to lock H264 sample buffer.");

  hr = buf->Unlock();
  CHECK_HR(hr, "Failed to unlock video sample buffer.");

  uint16_t pktSeqNum = *seqNum;

  for (UINT offset = 0; offset < frameLength;)
  {
    bool isLast = ((offset + RTP_MAX_PAYLOAD) >= frameLength); // Note can be first and last packet at same time if a small frame.
    UINT payloadLength = !isLast ? RTP_MAX_PAYLOAD : frameLength - offset;

    RtpHeader rtpHeader;
    rtpHeader.SyncSource = ssrc;
    rtpHeader.SeqNum = pktSeqNum++;
    rtpHeader.Timestamp = timestamp;
    rtpHeader.MarkerBit = 0;    // Set on first and last packet in frame.
    rtpHeader.PayloadType = RTP_PAYLOAD_ID;

    uint16_t h264Header = h264HeaderMiddle;

    if (isLast)
    {
      // This is the First AND Last RTP packet in the frame.
      h264Header = h264HeaderEnd;
      rtpHeader.MarkerBit = 1;
    }
    else if (offset == 0)
    {
      h264Header = h264HeaderStart;
      rtpHeader.MarkerBit = 1;
    }

    uint8_t* hdrSerialised = NULL;
    rtpHeader.Serialise(&hdrSerialised);

    int rtpPacketSize = RTP_HEADER_LENGTH + H264_RTP_HEADER_LENGTH + payloadLength;
    uint8_t* rtpPacket = (uint8_t*)malloc(rtpPacketSize);
    memcpy_s(rtpPacket, rtpPacketSize, hdrSerialised, RTP_HEADER_LENGTH);
    rtpPacket[RTP_HEADER_LENGTH] = (byte)(h264Header >> 8 & 0xff);
    rtpPacket[RTP_HEADER_LENGTH + 1] = (byte)(h264Header & 0xff);
    memcpy_s(&rtpPacket[RTP_HEADER_LENGTH + H264_RTP_HEADER_LENGTH], payloadLength, &frameData[offset], payloadLength);

    //printf("Sending RTP packet, length %d.\n", rtpPacketSize);

    sendto(socket, (const char *)rtpPacket, rtpPacketSize, 0, (sockaddr*)&dst, sizeof(dst));

    offset += payloadLength;

    free(hdrSerialised);
    free(rtpPacket);
  }

done:

  SAFE_RELEASE(buf);

  *seqNum = pktSeqNum;

  return hr;
}
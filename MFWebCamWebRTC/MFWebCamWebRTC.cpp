/******************************************************************************
* Filename: MFWebCamWebRTC.cpp
*
* Description:
* This file contains a C++ console application that captures the realtime video
* stream from a webcam using Windows Media Foundation and streams it to a WebRTC
* client.
*
* Dependencies:
* vcpkg install openssl libsrtp libvpx zlib
*
* To connect to the program the steps are below. The example uses the loopback address
* for the connection so the program and the browser need to be running on the same machine.
* The program only supports a single client and needs to be restarted at the end
* of each connection.
*
* 1. Build and run this program.
* 2. Open the mfwebrtc.html file in a browser.
* 3. Press the Start button on the web page and the webcam feed should appear.
*
* Browser Interop (as of 19 Jan 2020):
* - Works in Chrome.
* - Works in Edge Chromium.
* - Doesn't work in Firefox. Firefox requires a STUN binding request to be sent from the
*   remote peer before it will initiate the DTLS handshake. To send a binding request
*   the SDP answer from the browser is required (in order to get the ICE username and
*   password). To get the SDP there needs to be some kind of signaling transport such
*   a web socket. That would add a lot of noise to this example so stick to Chrome.
*
* Author:
* Aaron Clauson (aaron@sipsorcery.com)
*
* History:
* 14 Jan 2020	  Aaron Clauson	  Created, Dublin, Ireland.
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

#include <srtp2/srtp.h>
#include <openssl/bio.h>
#include <openssl/srtp.h>
#include <openssl/err.h>
#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>
#include <zlib.h>

#include <exception>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>

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
#define RTP_PAYLOAD_ID 100         // Needs to match the attribute set in the SDP (a=rtpmap:100 VP8/90000).
#define RTP_SSRC 337799
#define VP8_RTP_HEADER_LENGTH 1
#define RTP_LISTEN_PORT 8888      // The port this sample will listen on for an RTP connection from a WebRTC client.
#define DTLS_CERTIFICATE_FILE "localhost.pem"
#define DTLS_KEY_FILE "localhost_key.pem"
#define DTLS_COOKIE "sipsorcery"
#define RECEIVE_BUFFER_LENGTH 4096
#define SRTP_MASTER_KEY_KEY_LEN 16
#define SRTP_MASTER_KEY_SALT_LEN 14
#define ICE_USERNAME "EJYWWCUDJQLTXTNQRXEJ"
#define ICE_USERNAME_LENGTH 20
#define ICE_PASSWORD "SKYKPPYLTZOAVCLTGHDUODANRKSPOVQVKXJULOGG" // Must match the value in the SDP given to the client.
#define ICE_PASSWORD_LENGTH 40
#define SRTP_AUTH_KEY_LENGTH 10
#define VP8_TIMESTAMP_SPACING 3000

// Forward function definitions.
class StunMessage;
HRESULT SendRtpSample(SOCKET socket, sockaddr_in& dst, srtp_t* srtpSession, byte* frameData, size_t frameLength, uint32_t ssrc, uint32_t timestamp, uint16_t* seqNum);
void krx_ssl_info_callback(const SSL* ssl, int where, int ret);
int verify_cookie(SSL* ssl, unsigned char* cookie, unsigned int cookie_len);
int generate_cookie(SSL* ssl, unsigned char* cookie, unsigned int* cookie_len);
int StreamWebcam(SOCKET rtpSocket, sockaddr_in& dest, srtp_t* srtpSession);
void RtpSocketListen(SOCKET rtpSocket);
void SendStunBindingResponse(SOCKET rtpSocket, StunMessage& bindingRequest, sockaddr_in client);

#define SSL_WHERE_INFO(ssl, w, flag, msg) {                \
    if(w & flag) {                                         \
      printf("%20.20s", msg);                              \
      printf(" - %30.30s ", SSL_state_string_long(ssl));   \
      printf(" - %5.10s ", SSL_state_string(ssl));         \
      printf("\n");                                        \
	    }                                                    \
    } 

/**
* Minimal 12 byte RTP header structure. No facility for extensions etc.
*/
class RtpHeader
{
public:
  uint8_t Version = RTP_VERSION;   // 2 bits.
  uint8_t PaddingFlag = 0;        // 1 bit.
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

/* STUN message types needed for this example. */
enum class StunMessageTypes : uint16_t
{
  BindingRequest = 0x0001,
  BindingSuccessResponse = 0x0101,
  BindingErrorResponse = 0x0111,
};

/* STUN attribute types needed for this example. */
enum class StunAttributeTypes : uint16_t
{
  Username = 0x0006,
  Password = 0x0007,
  MessageIntegrity = 0x0008,
  Priority = 0x0024,
  XORMappedAddress = 0x0020,
  UseCandidate = 0x0025,
  FingerPrint = 0x8028,
};

/* Minimal STUN attribute. */
class StunAttribute
{
public:
  static const int HEADER_LENGTH = 4;
  static const int XORMAPPED_ADDRESS_ATTRIBUTE_LENGTH = 8;
  static const int MESSAGE_INTEGRITY_ATTRIBUTE_HMAC_LENGTH = 20;
  static const int FINGERPRINT_ATTRIBUTE_CRC32_LENGTH = 4;
  static const int FINGERPRINT_XOR = 0x5354554e;

  uint16_t Type = 0;            // 16 bits.
  uint16_t Length = 0;          // 16 bits.
  std::vector<uint8_t> Value;   // Variable length.
  uint16_t Padding = 0;         // Attributes start on 32 bit word boundaries. 

  StunAttribute()
  {}

  StunAttribute(StunAttributeTypes type, std::vector<uint8_t> val)
  {
    Type = (uint16_t)type;
    Length = val.size();
    Padding = (Length % 4 != 0) ? 4 - (Length % 4) : 0;
    Value = val;
  }

  void Deserialise(const uint8_t* buffer, int bufferLength)
  {
    if (bufferLength < HEADER_LENGTH) {
      throw std::runtime_error("Could not deserialise STUN attribute, buffer too small.");
    }
    else {
      Type = ((buffer[0] << 8) & 0xff00) + buffer[1];
      Length = ((buffer[2] << 8) & 0xff00) + buffer[3];
      Padding = (Length % 4 != 0) ? 4 - (Length % 4) : 0;
      Value.resize(Length);
      memcpy_s(Value.data(), Length, buffer + HEADER_LENGTH, Length);
    }
  }

  static StunAttribute GetXorMappedAddrAttribute(uint8_t addrFamily, uint16_t port, uint32_t address, const uint8_t * magicCookie)
  {
    std::vector<uint8_t> val(XORMAPPED_ADDRESS_ATTRIBUTE_LENGTH);

    val[0] = 0x00;
    val[1] = addrFamily == AF_INET ? 0x01 : 0x02;
    val[2] = (port >> 8) & 0xff ^ *magicCookie;
    val[3] = port & 0xff ^ *(magicCookie + 1);
    val[4] = (address >> 24) & 0xff ^ *magicCookie;
    val[5] = (address >> 16) & 0xff ^ *(magicCookie + 1);
    val[6] = (address >> 8) & 0xff ^ *(magicCookie + 2);
    val[7] = address & 0xff ^ *(magicCookie + 3);

    StunAttribute att(StunAttributeTypes::XORMappedAddress, val);
    return att;
  }

  static StunAttribute GetMessageIntegrityAttribute()
  {
    std::vector<uint8_t> emptyHmac(MESSAGE_INTEGRITY_ATTRIBUTE_HMAC_LENGTH, 0x00);
    StunAttribute att(StunAttributeTypes::MessageIntegrity, emptyHmac);
    return att;
  }

  static StunAttribute GetFingerprintAttribute()
  {
    std::vector<uint8_t> emptyFingerprint(FINGERPRINT_ATTRIBUTE_CRC32_LENGTH, 0x00);
    StunAttribute att(StunAttributeTypes::FingerPrint, emptyFingerprint);
    return att;
  }
};

class StunMessage
{
public:
  static const int HEADER_LENGTH = 20;
  static inline constexpr const uint8_t MAGIC_COOKIE_BYTES[] = { 0x21, 0x12, 0xA4, 0x42 };
  static const int TRANSACTION_ID_LENGTH = 12;
  static const uint8_t STUN_INITIAL_BYTE_MASK = 0xc0;

  // Header fields.
  uint16_t Type = 0;                              // 12 bits.
  uint16_t Length = 0;                            // 18 bits.
  uint8_t TransactionID[TRANSACTION_ID_LENGTH];   // 96 bits.

  std::vector<StunAttribute> Attributes;

  StunMessage()
  {}

  StunMessage(StunMessageTypes messageType)
  {
    Type = (uint16_t)messageType;
  }

  void AddXorMappedAttribute(uint8_t addrFamily, uint16_t port, uint32_t address, const uint8_t* magicCookie)
  {
    auto xorAddrAttribute = StunAttribute::GetXorMappedAddrAttribute(addrFamily, port, address, magicCookie);
    Attributes.push_back(xorAddrAttribute);
  }

  /**
  * Add as the second last attribute.
  */
  void AddHmacAttribute(const char* icePwd, int icePwdLen)
  {
    auto hmacAttribute = StunAttribute::GetMessageIntegrityAttribute();
    Attributes.push_back(hmacAttribute);

    uint8_t* respBuffer = nullptr;

    // The message integrity attribute doesn't get included in the HMAC.
    int respBufferLength = Serialise(&respBuffer) - StunAttribute::HEADER_LENGTH - StunAttribute::MESSAGE_INTEGRITY_ATTRIBUTE_HMAC_LENGTH;

    //std::cout << "HMAC input: " << HexStr(respBuffer, respBufferLength) << std::endl;

    UINT hmacLength = StunAttribute::MESSAGE_INTEGRITY_ATTRIBUTE_HMAC_LENGTH;
    std::vector<uint8_t> hmac(StunAttribute::MESSAGE_INTEGRITY_ATTRIBUTE_HMAC_LENGTH);

    HMAC(EVP_sha1(), icePwd, icePwdLen, respBuffer, respBufferLength, hmac.data(), &hmacLength);

    free(respBuffer);

    Attributes.back().Value = hmac;
  }

  /**
  * Add as the last attribute.
  */
  void AddFingerprintAttribute()
  {
    auto fingerprintAttribute = StunAttribute::GetFingerprintAttribute();
    Attributes.push_back(fingerprintAttribute);

    uint8_t* respBuffer = nullptr;

    // The fingerprint attribute doesn't get included in the CRC.
    int respBufferLength = Serialise(&respBuffer) - StunAttribute::HEADER_LENGTH - StunAttribute::FINGERPRINT_ATTRIBUTE_CRC32_LENGTH;

    //std::cout << "Fingerprint input: " << HexStr(respBuffer, respBufferLength) << std::endl;

    // Set the last 4 bytes with the fingerprint CRC.
    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const unsigned char*)respBuffer, respBufferLength);
    crc = crc ^ StunAttribute::FINGERPRINT_XOR;

    auto crcBuffer = Attributes.back().Value.data();

    crcBuffer[0] = (crc >> 24) & 0xff;
    crcBuffer[1] = (crc >> 16) & 0xff;
    crcBuffer[2] = (crc >> 8) & 0xff;
    crcBuffer[3] = crc & 0xff;
  }

  int Serialise(uint8_t** buf)
  {
    uint16_t messageLength = 0;
    for (auto att : Attributes) {
      messageLength += att.Length + att.Padding + StunAttribute::HEADER_LENGTH;
    }

    *buf = (uint8_t*)calloc(messageLength + HEADER_LENGTH, 1);

    // Serialise header.
    *(*buf) = (Type >> 8) & 0x03;
    *(*buf + 1) = Type & 0xff;
    *(*buf + 2) = (messageLength >> 8) & 0xff;
    *(*buf + 3) = messageLength & 0xff;
    // Magic Cookie
    *(*buf + 4) = MAGIC_COOKIE_BYTES[0];
    *(*buf + 5) = MAGIC_COOKIE_BYTES[1];
    *(*buf + 6) = MAGIC_COOKIE_BYTES[2];
    *(*buf + 7) = MAGIC_COOKIE_BYTES[3];
    // TransactionID.
    int bufPosn = 8;
    while (bufPosn < HEADER_LENGTH) {
      *(*buf + bufPosn) = TransactionID[bufPosn - 8];
      bufPosn++;
    }

    // Serialise attributes.
    bufPosn = HEADER_LENGTH;
    for (auto att : Attributes) {
      *(*buf + bufPosn++) = (att.Type) >> 8 & 0xff;
      *(*buf + bufPosn++) = att.Type & 0xff;
      *(*buf + bufPosn++) = (att.Length) >> 8 & 0xff;
      *(*buf + bufPosn++) = att.Length & 0xff;
      memcpy_s(*buf + bufPosn, att.Length, att.Value.data(), att.Length);
      bufPosn += att.Length + att.Padding;
    }

    return messageLength + HEADER_LENGTH;
  }

  void Deserialise(const uint8_t* buffer, int bufferLength)
  {
    if ((buffer[0] & STUN_INITIAL_BYTE_MASK) != 0) {
      throw std::runtime_error("Could not deserialise STUN header, invalid first byte.");
    }
    else if (bufferLength < HEADER_LENGTH) {
      throw std::runtime_error("Could not deserialise STUN header, buffer too small.");
    }
    else {
      Type = ((buffer[0] << 8) & 0xff00) + buffer[1];
      Length = ((buffer[2] << 8) & 0xff00) + buffer[3];
      memcpy_s(TransactionID, TRANSACTION_ID_LENGTH, &buffer[8], TRANSACTION_ID_LENGTH);
    }

    int bufPosn = HEADER_LENGTH;

    while (bufPosn < bufferLength) {
      StunAttribute att;
      att.Deserialise(buffer + bufPosn, bufferLength - bufPosn);
      Attributes.push_back(att);

      bufPosn += att.HEADER_LENGTH + att.Length + att.Padding;
    }
  }
};

int main()
{
  // Socket variables.
  WSADATA wsaData;
  SOCKET rtpSocket = INVALID_SOCKET;
  sockaddr_in service, dest;
  unsigned char recvBuffer[RECEIVE_BUFFER_LENGTH];
  sockaddr_in clientAddr;
  int clientAddrLen = sizeof(clientAddr);

  // DTLS variables.
  SSL_CTX* ctx = nullptr;		/* main ssl context */
  SSL* ssl = nullptr;       /* the SSL* which represents a "connection" */
  BIO* bio = nullptr;

  // SRTP variables.
  unsigned char dtls_buffer[SRTP_MASTER_KEY_KEY_LEN * 2 + SRTP_MASTER_KEY_SALT_LEN * 2];
  unsigned char client_write_key[SRTP_MASTER_KEY_KEY_LEN + SRTP_MASTER_KEY_SALT_LEN];
  unsigned char server_write_key[SRTP_MASTER_KEY_KEY_LEN + SRTP_MASTER_KEY_SALT_LEN];
  size_t keyMaterialOffset = 0;
  srtp_policy_t* srtpPolicy = nullptr;
  srtp_t* srtpSession = nullptr;

  try {

    // Initialise Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
      printf("WSAStartup failed: %d\n", iResult);
      goto done;
    }

    // Initialise OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();

    // Dump any openssl errors.
    ERR_print_errors_fp(stderr);

    // Initialise libsrtp.
    srtp_init();

    //------
    // Set up single UDP socket that will do all send/receive with the browser.
    //------
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = INADDR_ANY;
    service.sin_port = htons(RTP_LISTEN_PORT);

    rtpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (rtpSocket == INVALID_SOCKET) {
      wprintf(L"socket function failed with error: %u\n", WSAGetLastError());
      goto done;
    }

    iResult = bind(rtpSocket, (SOCKADDR*)&service, sizeof (service));
    if (iResult == SOCKET_ERROR) {
      wprintf(L"bind failed with error %u\n", WSAGetLastError());
      closesocket(rtpSocket);
      goto done;
    }

    printf("Waiting for browser connection...\n");

    //------
    // STUN
    //------
    int recvResult = recvfrom(rtpSocket, (char*)recvBuffer, RECEIVE_BUFFER_LENGTH, 0, (sockaddr*)&clientAddr, &clientAddrLen);
    if (recvResult == SOCKET_ERROR) {
      wprintf(L"recvfrom failed with error %d\n", WSAGetLastError());
      throw std::runtime_error("Initial receive when waiting for STUN packet failed.");
    }
    else {
      printf("Received %d bytes from %s:%d.\n", recvResult, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
      StunMessage stunMsg;
      stunMsg.Deserialise(recvBuffer, recvResult);

      printf("STUN message received, type %d, message length %d.\n", stunMsg.Type, stunMsg.Length);

       // Send binding success response. This response will trigger the browser to start the DTLS handshake.
      if (stunMsg.Type == (uint16_t)StunMessageTypes::BindingRequest) {
        printf("Sending initial STUN binding response.\n");
        SendStunBindingResponse(rtpSocket, stunMsg, clientAddr);
      }
    }

    //------
    // DTLS
    //------
    ctx = SSL_CTX_new(DTLS_method());	// Copes with DTLS 1.0 and 1.2.
    if (!ctx) {
      printf("Error: cannot create SSL_CTX.\n");
      goto done;
    }

    int r = SSL_CTX_set_cipher_list(ctx, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
    if (r != 1) {
      printf("Error: cannot set the cipher list.\n");
      goto done;
    }

    /* enable srtp */
    r = SSL_CTX_set_tlsext_use_srtp(ctx, "SRTP_AES128_CM_SHA1_80");
    if (r != 0) {
      printf("Error: cannot setup srtp.\n");
      goto done;
    }

    /* certificate file; contains also the public key */
    r = SSL_CTX_use_certificate_file(ctx, DTLS_CERTIFICATE_FILE, SSL_FILETYPE_PEM);
    if (r != 1) {
      printf("Error: cannot load certificate file.\n");
      goto done;
    }

    /* load private key */
    r = SSL_CTX_use_PrivateKey_file(ctx, DTLS_KEY_FILE, SSL_FILETYPE_PEM);
    if (r != 1) {
      printf("Error: cannot load private key file.\n");
      goto done;
    }

    /* check if the private key is valid */
    r = SSL_CTX_check_private_key(ctx);
    if (r != 1) {
      printf("Error: checking the private key failed.\n");
      goto done;
    }

    SSL_CTX_set_cookie_generate_cb(ctx, generate_cookie);
    SSL_CTX_set_cookie_verify_cb(ctx, verify_cookie);
    SSL_CTX_set_ecdh_auto(ctx, 1);                        // Needed for FireFox DTLS negotiation.
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);    // The client doesn't have to send it's certificate.

    // DTLS context now created. Accept new clients and do the handshake.
    bio = BIO_new_dgram(rtpSocket, BIO_NOCLOSE);
    ssl = SSL_new(ctx);
    SSL_set_bio(ssl, bio, bio);
    SSL_set_info_callback(ssl, krx_ssl_info_callback);    // info callback.
    SSL_set_accept_state(ssl);

    DTLSv1_listen(ssl, &clientAddr);

    printf("New DTLS client connection.\n");

    // Attempt to complete the DTLS handshake
    // If successful, the DTLS link state is initialized internally
    if (SSL_accept(ssl) <= 0) {
      printf("Failed to complete SSL handshake.\n");
      goto done;
    }
    else {
      printf("DTLS Handshake completed.\n");
    }

    //------
    // SRTP
    //------
    const char* label = "EXTRACTOR-dtls_srtp";

    r = SSL_export_keying_material(ssl,
      dtls_buffer,
      sizeof(dtls_buffer),
      label,
      strlen(label),
      NULL,
      0,
      0);
    if (r != 1) {
      printf("Error: exporting DTLS key material.\n");
      goto done;
    }

    memcpy(&client_write_key[0], &dtls_buffer[keyMaterialOffset], SRTP_MASTER_KEY_KEY_LEN);
    keyMaterialOffset += SRTP_MASTER_KEY_KEY_LEN;
    memcpy(&server_write_key[0], &dtls_buffer[keyMaterialOffset], SRTP_MASTER_KEY_KEY_LEN);
    keyMaterialOffset += SRTP_MASTER_KEY_KEY_LEN;
    memcpy(&client_write_key[SRTP_MASTER_KEY_KEY_LEN], &dtls_buffer[keyMaterialOffset], SRTP_MASTER_KEY_SALT_LEN);
    keyMaterialOffset += SRTP_MASTER_KEY_SALT_LEN;
    memcpy(&server_write_key[SRTP_MASTER_KEY_KEY_LEN], &dtls_buffer[keyMaterialOffset], SRTP_MASTER_KEY_SALT_LEN);

    srtp_policy_t* srtpPolicy = new srtp_policy_t();
    srtp_t* srtpSession = new srtp_t();

    srtp_crypto_policy_set_rtp_default(&srtpPolicy->rtp);
    srtp_crypto_policy_set_rtcp_default(&srtpPolicy->rtcp);

    /* Init transmit direction */
    srtpPolicy->key = server_write_key;

    srtpPolicy->ssrc.value = 0;
    srtpPolicy->window_size = 128;
    srtpPolicy->allow_repeat_tx = 0;
    srtpPolicy->ssrc.type = ssrc_any_outbound;
    srtpPolicy->next = NULL;

    auto err = srtp_create(srtpSession, srtpPolicy);
    if (err != srtp_err_status_ok) {
      printf("Unable to create SRTP session.\n");
      goto done;
    }
    else {
      printf("SRTP session created.\n");
    }

    // Need to keep responding to STUN binding requests or the connection will be flagged as disconnected.
    std::thread listenThread(RtpSocketListen, rtpSocket);

    // The connection with the browser should now be negotiated.
    // Webcam sample streaming can commence.
    StreamWebcam(rtpSocket, clientAddr, srtpSession);

    delete(srtpSession);
    delete(srtpPolicy);
  }
  catch (std::exception & excp) {
    std::cout << "Exception: " << excp.what() << std::endl;
  }

done:

  ERR_print_errors_fp(stderr);

  printf("Cleanup.\n");

  // OpenSSL cleanup.
  if (ctx != nullptr) {
    SSL_CTX_free(ctx);
  }

  if (ssl != nullptr) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
  }

  ERR_remove_state(0);
  //ENGINE_cleanup();
  //CONF_modules_unload(1);
  ERR_free_strings();
  EVP_cleanup();
  sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
  CRYPTO_cleanup_all_ex_data();

  // Winsock cleanup
  closesocket(rtpSocket);
  WSACleanup();
}

void SendStunBindingResponse(SOCKET rtpSocket, StunMessage & bindingRequest, sockaddr_in client)
{
  StunMessage stunBindingResp(StunMessageTypes::BindingSuccessResponse);
  std::copy(bindingRequest.TransactionID, bindingRequest.TransactionID + StunMessage::TRANSACTION_ID_LENGTH, stunBindingResp.TransactionID);

  // Add required attributes.
  stunBindingResp.AddXorMappedAttribute(client.sin_family, ntohs(client.sin_port), ntohl(client.sin_addr.s_addr), StunMessage::MAGIC_COOKIE_BYTES);
  stunBindingResp.AddHmacAttribute(ICE_PASSWORD, ICE_PASSWORD_LENGTH);
  stunBindingResp.AddFingerprintAttribute();

  uint8_t* respBuffer = nullptr;
  int respBufferLength = stunBindingResp.Serialise(&respBuffer);

  //printf("Sending STUN response packet, length %d.\n", respBufferLength);

  sendto(rtpSocket, (const char*)respBuffer, respBufferLength, 0, (sockaddr*)&client, sizeof(client));

  free(respBuffer);
}

/*

From RFC5764:
                   +----------------+
                   | 127 < B < 192 -+--> forward to RTP
                   |                |
       packet -->  |  19 < B < 64  -+--> forward to DTLS
                   |                |
                   |       B < 2   -+--> forward to STUN
                   +----------------+
*/
void RtpSocketListen(SOCKET rtpSocket)
{
  unsigned char recvBuffer[RECEIVE_BUFFER_LENGTH];
  sockaddr_in clientAddr;
  int clientAddrLen = sizeof(clientAddr);

  printf("RTP socket listener started.\n");

  while (true) {
    int recvResult = recvfrom(rtpSocket, (char*)recvBuffer, RECEIVE_BUFFER_LENGTH, 0, (sockaddr*)&clientAddr, &clientAddrLen);
    if (recvResult == SOCKET_ERROR) {
      wprintf(L"recvfrom failed with error %d\n", WSAGetLastError());
    }
    else {
      //printf("Received %d bytes from %s:%d.\n", recvResult, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

      // See section 5.1.2 RFC5764.
      if (recvBuffer[0] == 0x00 || recvBuffer[0] == 0x01) {
        // STUN packet.
        //printf("STUN packet received.\n");
        StunMessage stunMsg;
        stunMsg.Deserialise(recvBuffer, recvResult);

        // Send binding success response.
        if (stunMsg.Type == (uint16_t)StunMessageTypes::BindingRequest) {
          SendStunBindingResponse(rtpSocket, stunMsg, clientAddr);
        }
      }
      else if (recvBuffer[0] >= 128 && recvBuffer[0] <= 191) {
        // RTP/RTCP packet.
        //printf("RTP or RTCP packet received.\n");
      }
      else if (recvBuffer[0] >= 20 && recvBuffer[0] <= 63) {
        // DTLS packet.
        //printf("DTLS packet received.\n");
      }
      else {
        printf("Unknown packet type received.\n");
      }
    }
  }
}

int StreamWebcam(SOCKET rtpSocket, sockaddr_in& dest, srtp_t* srtpSession)
{
  IMFMediaSource* pVideoSource = NULL;
  IMFSourceReader* pVideoReader = NULL;
  WCHAR* webcamFriendlyName;
  IMFMediaType* pSrcOutMediaType = NULL;
  UINT friendlyNameLength = 0;
  LONG stride = 0;

  vpx_codec_ctx_t* vpxCodec = nullptr;
  vpx_image_t* rawImage = nullptr;

  uint16_t rtpSsrc = RTP_SSRC; // Supposed to be pseudo-random.
  uint16_t rtpSeqNum = 0;
  uint32_t rtpTimestamp = 0;

  /*CHECK_HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
    "COM initialisation failed.");*/

  CHECK_HR(MFStartup(MF_VERSION),
    "Media Foundation initialisation failed.");

  // Get video capture device.
  CHECK_HR(GetVideoSourceFromDevice(WEBCAM_DEVICE_INDEX, &pVideoSource, &pVideoReader),
    "Failed to get webcam video source.");

  // Note the webcam needs to support this media type. 
  MFCreateMediaType(&pSrcOutMediaType);
  CHECK_HR(pSrcOutMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set major video type.");
  CHECK_HR(pSrcOutMediaType->SetGUID(MF_MT_SUBTYPE, WMMEDIASUBTYPE_I420), "Failed to set video sub type to I420.");
  CHECK_HR(MFSetAttributeRatio(pSrcOutMediaType, MF_MT_FRAME_RATE, OUTPUT_FRAME_RATE, 1), "Failed to set frame rate on source reader out type.");
  CHECK_HR(MFSetAttributeSize(pSrcOutMediaType, MF_MT_FRAME_SIZE, OUTPUT_FRAME_WIDTH, OUTPUT_FRAME_HEIGHT), "Failed to set frame size.");

  CHECK_HR(pVideoReader->SetCurrentMediaType(0, NULL, pSrcOutMediaType),
    "Failed to set media type on source reader.");

  printf("%s\n", GetMediaTypeDescription(pSrcOutMediaType).c_str());

  CHECK_HR(GetDefaultStride(pSrcOutMediaType, &stride),
    "Failed to get stride from source output media type.");

  printf("Stride %d.\n", stride);

  // Initialise the VPX encoder.
  vpxCodec = new vpx_codec_ctx_t();
  rawImage = new vpx_image_t();

  vpx_codec_enc_cfg_t vpxConfig;
  vpx_codec_err_t res;

  printf("Using %s\n", vpx_codec_iface_name(vpx_codec_vp8_cx()));

  /* Populate encoder configuration */
  res = vpx_codec_enc_config_default((vpx_codec_vp8_cx()), &vpxConfig, 0);

  if (res) {
    printf("Failed to get VPX codec config: %s\n", vpx_codec_err_to_string(res));
    goto done;
  }
  else {
    vpx_img_alloc(rawImage, VPX_IMG_FMT_I420, OUTPUT_FRAME_WIDTH, OUTPUT_FRAME_HEIGHT, stride);

    vpxConfig.g_w = OUTPUT_FRAME_WIDTH;
    vpxConfig.g_h = OUTPUT_FRAME_HEIGHT;
    vpxConfig.rc_target_bitrate = 300; // in kbps.
    vpxConfig.rc_min_quantizer = 20; // 50;
    vpxConfig.rc_max_quantizer = 30; // 60;
    vpxConfig.g_pass = VPX_RC_ONE_PASS;
    vpxConfig.rc_end_usage = VPX_CBR;
    vpxConfig.g_error_resilient = VPX_ERROR_RESILIENT_DEFAULT;
    vpxConfig.g_lag_in_frames = 0;
    vpxConfig.rc_resize_allowed = 0;
    vpxConfig.kf_max_dist = 20;

    /* Initialize codec */
    if (vpx_codec_enc_init(vpxCodec, (vpx_codec_vp8_cx()), &vpxConfig, 0)) {
      printf("Failed to initialize libvpx encoder.\n");
      goto done;
    }
  }

  // Ready to go.

  printf("Reading video samples from webcam.\n");

  IMFSample* pVideoSample = NULL;
  DWORD streamIndex = 0, flags = 0, sampleFlags = 0;
  LONGLONG llVideoTimeStamp, llSampleDuration;
  int sampleCount = 0;
  UINT vp8Timestamp = 0;

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

    if (pVideoSample)
    {
      CHECK_HR(pVideoSample->SetSampleTime(llVideoTimeStamp), "Error setting the video sample time.");
      CHECK_HR(pVideoSample->GetSampleDuration(&llSampleDuration), "Error getting video sample duration.");
      CHECK_HR(pVideoSample->GetSampleFlags(&sampleFlags), "Error getting sample flags.");

      IMFMediaBuffer* buf = NULL;
      DWORD frameLength = 0, buffCurrLen = 0, buffMaxLen = 0;
      byte* frameData = NULL;

      CHECK_HR(pVideoSample->ConvertToContiguousBuffer(&buf),
        "ConvertToContiguousBuffer failed.");

      CHECK_HR(buf->GetCurrentLength(&frameLength),
        "Get buffer length failed.");

      CHECK_HR(buf->Lock(&frameData, &buffMaxLen, &buffCurrLen),
        "Failed to lock video sample buffer.");

      //printf("Sample count %d, Sample flags %d, sample duration %I64d, sample time %I64d\n", sampleCount, sampleFlags, llSampleDuration, llVideoTimeStamp);
      vpx_image_t* const img = vpx_img_wrap(rawImage, VPX_IMG_FMT_I420, OUTPUT_FRAME_WIDTH, OUTPUT_FRAME_HEIGHT, 1, frameData);

      const vpx_codec_cx_pkt_t* pkt;
      vpx_enc_frame_flags_t flags = 0;

      if (vpx_codec_encode(vpxCodec, rawImage, sampleCount, 1, flags, VPX_DL_REALTIME)) {
        printf("VPX codec failed to encode the frame.\n");
        goto done;
      }
      else {
        vpx_codec_iter_t iter = NULL;

        while ((pkt = vpx_codec_get_cx_data(vpxCodec, &iter))) {
          switch (pkt->kind) {
          case VPX_CODEC_CX_FRAME_PKT:
            SendRtpSample(rtpSocket, dest, srtpSession, (byte *)pkt->data.raw.buf, pkt->data.raw.sz, rtpSsrc, vp8Timestamp, &rtpSeqNum);
            break;
          default:
            break;
          }
        }
      }

      vpx_img_free(img);

      CHECK_HR(buf->Unlock(),
        "Failed to unlock video sample buffer.");  

      SAFE_RELEASE(buf);

      vp8Timestamp += VP8_TIMESTAMP_SPACING;
    }
    // *****

    sampleCount++;

    // Note: Apart from memory leak issues if the media samples are not released the videoReader->ReadSample
    // blocks when it is unable to allocate a new sample.
    SAFE_RELEASE(pVideoSample);
  }


done:

  printf("finished.\n");
  auto c = getchar();

  delete(vpxCodec);
  delete(rawImage);

  SAFE_RELEASE(pVideoSource);
  SAFE_RELEASE(pVideoReader);
  SAFE_RELEASE(pSrcOutMediaType);

  WSACleanup();

  return 0;
}

HRESULT SendRtpSample(SOCKET socket, sockaddr_in& dst, srtp_t* srtpSession, byte* frameData, size_t frameLength, uint32_t ssrc, uint32_t timestamp, uint16_t* seqNum)
{
  HRESULT hr = S_OK;

  uint16_t pktSeqNum = *seqNum;

  for (UINT offset = 0; offset < frameLength;)
  {
    bool isLast = ((offset + RTP_MAX_PAYLOAD) >= frameLength); // Note can be first and last packet at same time if a small frame.
    UINT payloadLength = !isLast ? RTP_MAX_PAYLOAD : frameLength - offset;

    RtpHeader rtpHeader;
    rtpHeader.SyncSource = ssrc;
    rtpHeader.SeqNum = pktSeqNum++;
    rtpHeader.Timestamp = timestamp;
    rtpHeader.MarkerBit = (isLast) ? 1 : 0;    // Marker bit gets set on last packet in frame.
    rtpHeader.PayloadType = RTP_PAYLOAD_ID;

    uint8_t* hdrSerialised = NULL;
    rtpHeader.Serialise(&hdrSerialised);

    int rtpPacketSize = RTP_HEADER_LENGTH + VP8_RTP_HEADER_LENGTH + payloadLength;
    int srtpPacketSize = rtpPacketSize + SRTP_AUTH_KEY_LENGTH;
    uint8_t* rtpPacket = (uint8_t*)malloc(srtpPacketSize);
    memcpy_s(rtpPacket, rtpPacketSize, hdrSerialised, RTP_HEADER_LENGTH);
    rtpPacket[RTP_HEADER_LENGTH] = (offset == 0) ? 0x10 : 0x00 ; // Set the VP8 header byte.
    memcpy_s(&rtpPacket[RTP_HEADER_LENGTH + VP8_RTP_HEADER_LENGTH], payloadLength, &frameData[offset], payloadLength);

    //printf("Sending RTP packet, length %d.\n", rtpPacketSize);

    auto protRes = srtp_protect(*srtpSession, rtpPacket, &rtpPacketSize);
    if (protRes != srtp_err_status_ok) {
      printf("SRTP protect failed with error code %d.\n", protRes);
      hr = E_FAIL;
      break;
    }

    sendto(socket, (const char*)rtpPacket, srtpPacketSize, 0, (sockaddr*)&dst, sizeof(dst));

    offset += payloadLength;

    free(hdrSerialised);
    free(rtpPacket);
  }

done:

  *seqNum = pktSeqNum;

  return hr;
}

int verify_cookie(SSL* ssl, unsigned char* cookie, unsigned int cookie_len)
{
  // Accept any cookie.
  return 1;
}

int generate_cookie(SSL* ssl, unsigned char* cookie, unsigned int* cookie_len)
{
  int cookieLength = sizeof(DTLS_COOKIE);
  *cookie_len = cookieLength;
  memcpy(cookie, (unsigned char*)DTLS_COOKIE, cookieLength);
  return 1;
}

void krx_ssl_info_callback(const SSL* ssl, int where, int ret)
{
  if (ret == 0) {
    printf("-- krx_ssl_info_callback: error occured.\n");
    return;
  }

  SSL_WHERE_INFO(ssl, where, SSL_CB_LOOP, "LOOP");
  SSL_WHERE_INFO(ssl, where, SSL_CB_HANDSHAKE_START, "HANDSHAKE START");
  SSL_WHERE_INFO(ssl, where, SSL_CB_HANDSHAKE_DONE, "HANDSHAKE DONE");
}

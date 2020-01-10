# Unofficial Windows Media Foundation Samples

Official samples are available [here](https://github.com/microsoft/Windows-classic-samples/tree/master/Samples/Win7Samples/multimedia/mediafoundation) and a brief overview web page [here](https://docs.microsoft.com/en-us/windows/win32/medfound/media-foundation-sdk-samples).

A set of minimal sample apps that demonstrate how to use certain parts of Microsoft's Windows Media Foundation API. The original motivation for these samples was an attempt to find a way to stream audio and video from a webcam, encoded as H264 and/or VP8, over RTP and then render at the remote destination. As of **January 2020** this goal has **NOT** been achieved.

### Rendering

 - MFAudio - Play audio from file on speaker.
 
 - MFTopology - Plays an mp4 file using the Enhanced Video Renderer in a separate Window. TODO: Audio not working.
 
 - MFBitmapToEVR - Displays a byte array representing a bitmap on the Enhanced Video Renderer.
 
 - MFBitmapMFTToEVR - Performs a colour conversion on a bitmap byte array and then dislays on the Enhanced Video Renderer. 
 
 - MFVideoEVR - Display video from an mp4 file in Window WITHOUT using a topology. Write samples to video renderer directly from buffer.
 
 - MFVideoEVRWebcam - Same as the `MFVideoEVR` sample but replacing the file source with a webcam.
 
 - MFVideoEVRWebcamMFT - Same as the `MFVideoEVRWebcam` sample but manually wires up a color conversion MFT transform instead of setting MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING on the video source reader.  
 
 - WpfMediaUWA - Initial foray into how Media Foundation can work with WPF in a Universal Windows Application (UWA). UWA is currently impractical due to [deployment contraints](https://docs.microsoft.com/en-us/windows/apps/desktop/choose-your-platform), i.e. Windows Store only. Hopefully in 2020 with the introduction of [Windows UI 3.0](https://docs.microsoft.com/en-us/uwp/toolkits/) using the types of controls in this sample will become practical.
 
### Plumbing

 - MFListTransforms - Lists the available MFT Transforms to convert between two media types.
 
 - MFMP4ToYUVWithMFT - Reads H264 encoded video frames from an mp4 file and decodes them to a YUV pixel format and dumps them to an output file.
 
 - MFCaptureRawFramesToFile - Captures 100 samples from default webcam to file.
 
 - MFWebCamToFile - Captures 100 samples from default webcam to an mp4 file.

### Video Capture -> H264 -> RTP

 - MFWebCamToH264Buffer - Captures the video stream from a webcam to an H264 byte array by directly using the MFT H264 Encoder.

 - MFH264RoundTrip - Captures video frames, H264 encode to byte array, decode to YUV (replicates encode, transmit, decode).

 - MFWebCamRtp - **Work In Progress**. Stream mp4 file to ffplay with RTP stream.
 
 

 


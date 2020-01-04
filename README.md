# Unofficial Windows Media Foundation Samples

Official samples are available [here](https://github.com/microsoft/Windows-classic-samples/tree/master/Samples/Win7Samples/multimedia/mediafoundation) including a brief overview web page [here](https://docs.microsoft.com/en-us/windows/win32/medfound/media-foundation-sdk-samples).

A set of minimal sample apps that demonstrate how to use certain parts of Microsoft's Windows Media Foundation API. The original motivation for these samples was the attempt to find a way to stream audio and video from a webcam, encoded as H264 and/or VP8, over RTP and then ideally render it at the remote destination. 

As of **January 2020** the goal has not been achieved.

### Rendering

 - MFAudio - **Working**. Play audio from file on speaker.
 
 - MFTopology - **Working**. Plays an mp4 file using the Enhanced Video Renderer in a separate Window. TODO: Audio not working.
 
 - MFVideoEVR - **Not Working**. Display video in Window.
 
 - WpfMediaUWA - **Working**. Initial foray into how Media Foundation can work with WPF in a Universal Windows Application (UWA). UWA is currently impractical due to [deployment contraints](https://docs.microsoft.com/en-us/windows/apps/desktop/choose-your-platform), i.e. Windows Store only. Hopefully in 2020 with the introduction of [Windows UI 3.0](https://docs.microsoft.com/en-us/uwp/toolkits/) using the types of controls in this sample will become practical.
 
### Plumbing

 - MFListTransforms - **Working**. Lists the available MFT Transforms to convert between two media types.
 
 - MFMP4ToYUVWithMFT - **Working**. Reads H264 encoded video frames from an mp4 file and decodes them to a YUV pixel format and dumps them to an output file.
 
 - MFCaptureRawFramesToFile - **Working**. Captures 100 samples from default webcam to file. TODO: Video inverted.
 
 - MFWebCamToFile - **Working**. Captures 100 samples from default webcam to an mp4 file. TODO: Video inverted.
 

### Video Capture -> H264 -> RTP

 - MFWebCamToH264Buffer - **Working**. Captures the video stream from a webcam to an H264 byte array by directly using the MFT H264 Encoder.

 - MFH264RoundTrip - **Not Working**. Captures video frames, H264 encode to byte array, decode to YUV (replicates encode, transmit, decode).

 - MFWebCamRtp - **Not Working**. Stream mp4 file to ffplay with RTP stream.
 
 

 


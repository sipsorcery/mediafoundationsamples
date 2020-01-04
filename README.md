# Unofficial Windows Media Foundation Samples

Official samples are available [here](https://github.com/microsoft/Windows-classic-samples/tree/master/Samples/Win7Samples/multimedia/mediafoundation) including a brief overview web page [here](https://docs.microsoft.com/en-us/windows/win32/medfound/media-foundation-sdk-samples).

A set of minimal sample apps that demonstrate how to use certain parts of Microsoft's Windows Media Foundation API. The original motivation for these samples was the attempt to find a way to stream audio and video from a webcam, encoded as H264 and/or VP8, over RTP and then ideally render it at the remote destination. 

As of **January 2020** the goal has not been achieved.

 - MFAudio - **Working**. Play audio from file on speaker.
 
 - MFCaptureRawFramesToFile - **Working**. Captures 100 samples from default webcam to file. TODO: Video inverted.
 
 - MFH264RoundTrip - **Not Working**. Captures video frames, H264 encode to byte array, decode to YUV (replicates encode, transmit, decode).
 
 - MFListTransforms - **Working**. Lists the available MFT Transforms to convert between two media types.
 
 - MFMP4ToYUVWithMFT
 
 - MFTopology - **Working**. Plays an mp4 file using the Enhanced Video Renderer in a separate Window. TODO: Audio not working.
 
 - MFVideoEVR - **Not Working**. Display video in Window.
 
 - MFWebCamRtp - **Not Working**. Stream mp4 file to ffplay with RTP stream.
 
 - MFWebCamToFile - **Working**. Captures 100 samples from default webcam to an mp4 file. TODO: Video inverted.
 
 - MFWebCamToH264Buffer - 
 
 - WpfMediaUWA - **Working**. Initial foray into how Media Foundation can work with WPF in a Universal Windows Application (UWA). UWA is currently impractical due to [deployment contraints](https://docs.microsoft.com/en-us/windows/apps/desktop/choose-your-platform), i.e. Windows Store only. Hopefully in 2020 with the introduction of [Windows UI 3.0](https://docs.microsoft.com/en-us/uwp/toolkits/) using the types of controls in this sample will become practical.

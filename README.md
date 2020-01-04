# mediafoundationsamples
A  set of minimal sample apps that demonstrate how to use certain parts of Microsoft's Windows Media Foundation API.

 - MFAudio - **Working**. Play audio from file on speaker.
 
 - MFCaptureRawFramesToFile - **Working**. Captures 100 samples from default webcam to file. TODO: Video inverted.
 
 - MFH264RoundTrip - **Not Working**. Captures video frames, H264 encode to byte array, decode to YUV (replicates encode, transmit, decode).
 
 - MFListTransforms - **Working**. Lists the available MFT Transforms to convert between two media types.
 
 - MFMP4ToYUVWithMFT
 
 - MFTopology - **Working**. Plays an mp4 file using the Enhanced Video Renderer in a separate Window. TODO: Audio not working.
 
 - MFVideoEVR - **Not Working**. Display video in Window.
 
 - MFWebCamRtp - **Not Working**. Stream mp4 file to ffplay with RTP stream.
 
 - MFWebCamToFile - **Working**. Captures 100 samples from default webcam to an mp4 file. TODO: Video inverted.
 
 - MFWebCamToH264Buffer
 
 - WpfMediaUWA - **Working**. Initial foray into how Media Foundation can work with WPF in a Universal Windows Application (UWA). UWA is currently impractical due to [deployment contraints](https://docs.microsoft.com/en-us/windows/apps/desktop/choose-your-platform), i.e. Windows Store only. Hopefully in 2020 with the introduction of [Windows UI 3.0](https://docs.microsoft.com/en-us/uwp/toolkits/) using the types of controls in this sample will become practical.

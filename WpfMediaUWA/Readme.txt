Date: 29 Oct 2015
Author: Aaron Clauson
Solution Name: WpfMediaUWA.sln

Description:

Initial foray into how Media Foundation can work with WPF in a Universal Windows Application 
targetted for Windows 10.

The goal was to create a WPF application that could do two things:

1. Display the live feed from a webcam,
2. Display video from an H264 bitstream decoded by the Media Foundation.

(There was an intermediate step which was to display an RGB24 bitmap generated via
Media Foundation and that't the roving square stuff)

For once it was surprising how easy it was to get the sample working once the 
whole Universal Windows approach is understood a bit better. In particular using a webcam
in WPF is now literally a handful of lines of code which is a huge improvement on the
past (doing this on Windows 7 took literally hundreds of lines of code).

Getting the output from an mp4 file onto a WPF surface also turned out to be surprisingly
smooth expecially considering I haven't been able to acheive the same thing with a Win32
window handle despite a lot fo effort.
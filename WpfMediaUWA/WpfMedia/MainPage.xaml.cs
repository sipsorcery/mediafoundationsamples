using System;
using System.Diagnostics;
using System.Threading.Tasks;
using Windows.Media.Capture;
using Windows.Media.Core;
using Windows.Media.MediaProperties;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Navigation;

namespace WpfMedia
{
    public sealed partial class MainPage : Page
    {
        private const int WIDTH = 640;
        private const int HEIGHT = 480;
        private const int MP4_WIDTH = 640;
        private const int MP4_HEIGHT = 480;
        private const int FRAME_RATE = 30;

        private MediaCapture _mediaCapture;
        VideoStreamDescriptor _videoDesc;
        Windows.Media.Core.MediaStreamSource _mss;
        SurfaceGenerator.SampleMaker _sampleMaker;
        SurfaceGenerator.Mp4Sampler _mp4Sampler;

        public MainPage()
        {
            this.InitializeComponent();
        }

        protected async override void OnNavigatedTo(NavigationEventArgs e)
        {
            base.OnNavigatedTo(e);

            var folder = Windows.ApplicationModel.Package.Current.InstalledLocation;
            Debug.WriteLine("Installed location folder path " + folder.Path + ".");
            
            //InitialiseRovingSquareSampleMedia();

            InitialiseMp4FileMedia(folder.Path + @"\Assets\big_buck_bunny.mp4");

            await InitialiseWebCamAsync();
        }

        public void InitialiseRovingSquareSampleMedia()
        {
            VideoEncodingProperties videoProperties = VideoEncodingProperties.CreateUncompressed(MediaEncodingSubtypes.Bgra8, (uint)WIDTH, (uint)HEIGHT);
            _videoDesc = new VideoStreamDescriptor(videoProperties);
            _videoDesc.EncodingProperties.FrameRate.Numerator = FRAME_RATE;
            _videoDesc.EncodingProperties.FrameRate.Denominator = 1;
            _videoDesc.EncodingProperties.Bitrate = (uint)(1 * FRAME_RATE * WIDTH * HEIGHT * 4);

            _mss = new Windows.Media.Core.MediaStreamSource(_videoDesc);
            TimeSpan spanBuffer = new TimeSpan(0);
            _mss.BufferTime = spanBuffer;
            _mss.Starting += mss_Starting;
            _mss.SampleRequested += mss_SampleRequested;

            _sampleMaker = new SurfaceGenerator.SampleMaker();

            _remoteVideo.MediaFailed += _remoteVideo_MediaFailed;
            _remoteVideo.SetMediaStreamSource(_mss);
            _remoteVideo.Play();
        }

        public void InitialiseMp4FileMedia(string path)
        {
            try
            {
                VideoEncodingProperties videoProperties = VideoEncodingProperties.CreateH264();
                _videoDesc = new VideoStreamDescriptor(videoProperties);
                _videoDesc.EncodingProperties.FrameRate.Numerator = FRAME_RATE;
                _videoDesc.EncodingProperties.FrameRate.Denominator = 1;
                //_videoDesc.EncodingProperties.Bitrate = (uint)(1 * FRAME_RATE * MP4_WIDTH * MP4_HEIGHT * 4);

                _mss = new Windows.Media.Core.MediaStreamSource(_videoDesc);
                TimeSpan spanBuffer = new TimeSpan(0);
                _mss.BufferTime = spanBuffer;
                _mss.Starting += mp4_Starting;
                _mss.SampleRequested += mp4_SampleRequested;

                _mp4Sampler = new SurfaceGenerator.Mp4Sampler();
                
                _remoteVideo.MediaFailed += _remoteVideo_MediaFailed;
                _remoteVideo.SetMediaStreamSource(_mss);
                _remoteVideo.Play();
            }
            catch(Exception excp)
            {
                Debug.WriteLine("Exception InitialiseMp4FileMedia. " + excp);
            }
        }

        public async Task InitialiseWebCamAsync()
        {
            _mediaCapture = new MediaCapture();

            await _mediaCapture.InitializeAsync();

            _localCamera.Source = _mediaCapture;

            await _mediaCapture.StartPreviewAsync();
        }

        void mss_SampleRequested(Windows.Media.Core.MediaStreamSource sender, MediaStreamSourceSampleRequestedEventArgs args)
        {
            if (args.Request.StreamDescriptor is VideoStreamDescriptor)
            {
                _sampleMaker.GenerateSample(args.Request);
            }
        }

        void mp4_SampleRequested(Windows.Media.Core.MediaStreamSource sender, MediaStreamSourceSampleRequestedEventArgs args)
        {
            try
            {
                if (args.Request.StreamDescriptor is VideoStreamDescriptor)
                {
                    _mp4Sampler.GetSample(args.Request);
                }
            }
            catch(Exception excp)
            {
                Debug.WriteLine("Exception mp4_SampleRequeste. " + excp.Message);
            }
        }

        void mss_Starting(Windows.Media.Core.MediaStreamSource sender, MediaStreamSourceStartingEventArgs args)
        {
            Debug.WriteLine("Starting.");

            _sampleMaker.Initialize(_mss, _videoDesc);

            args.Request.SetActualStartPosition(new TimeSpan(0));
        }

        void mp4_Starting(Windows.Media.Core.MediaStreamSource sender, MediaStreamSourceStartingEventArgs args)
        {
            Debug.WriteLine("Starting.");

            //_sampleMaker.Initialize(_mss, _videoDesc);
            var folder = Windows.ApplicationModel.Package.Current.InstalledLocation;
            _mp4Sampler.Initialise(folder.Path + @"\Assets\big_buck_bunny.mp4");

            args.Request.SetActualStartPosition(new TimeSpan(0));
        }

        private void _remoteVideo_MediaFailed(object sender, ExceptionRoutedEventArgs e)
        {
            Debug.WriteLine("Load media failed. " + e.ErrorMessage);
        }
    }
}

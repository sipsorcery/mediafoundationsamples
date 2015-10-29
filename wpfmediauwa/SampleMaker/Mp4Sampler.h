#pragma once

namespace SurfaceGenerator
{
	public ref class Mp4Sampler sealed
	{
		public:
			Mp4Sampler();
			void Initialise(Platform::String^ path);
			void GetSample(Windows::Media::Core::MediaStreamSourceSampleRequest ^ request);

		private:
			IMFSourceReader * _videoReader = nullptr;
	};
}
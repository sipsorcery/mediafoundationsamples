#pragma once

namespace SurfaceGenerator
{
	public ref class SampleMaker sealed
	{
	public:
		SampleMaker();
		virtual ~SampleMaker();
		void Initialize(Windows::Media::Core::MediaStreamSource ^ mss, Windows::Media::Core::VideoStreamDescriptor ^ videoDesc);
		void GenerateSample(Windows::Media::Core::MediaStreamSourceSampleRequest ^ request);

	private:
		HRESULT ConvertPropertiesToMediaType(_In_ Windows::Media::MediaProperties::IMediaEncodingProperties ^mep, _Outptr_ IMFMediaType **ppMT);
		static HRESULT AddAttribute(_In_ GUID guidKey, _In_ Windows::Foundation::IPropertyValue ^value, _In_ IMFAttributes *pAttr);
		HRESULT GenerateSampleFrame(UINT32 ui32Width, UINT32 ui32Height, int iFrameRotation, IMFMediaBuffer * pBuffer);

		Microsoft::WRL::ComPtr<IMFVideoSampleAllocator> _spSampleAllocator;
		Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> _spDeviceManager;
		Microsoft::WRL::ComPtr<ID3D11Device> _spD3DDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> _spD3DContext;
		HANDLE _hDevice;
		ULONGLONG _ulVideoTimestamp;
		int _iVideoFrameNumber;
	};
}

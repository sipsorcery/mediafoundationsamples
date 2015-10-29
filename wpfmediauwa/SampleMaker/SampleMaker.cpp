#include "pch.h"
#include "SampleMaker.h"
#include <string.h>
#include <iostream>
#include <sstream>

namespace SurfaceGenerator
{
	SampleMaker::SampleMaker() :
		_hDevice(nullptr),
		_ulVideoTimestamp(0),
		_iVideoFrameNumber(0)
	{

	}

	SampleMaker::~SampleMaker()
	{
		if (_spDeviceManager.Get() != nullptr && _hDevice != nullptr)
		{
			_spDeviceManager->CloseDeviceHandle(_hDevice);
			_hDevice = nullptr;
		}

		if (_spSampleAllocator != nullptr)
		{
			_spSampleAllocator->UninitializeSampleAllocator();
		}
	}

	void SampleMaker::Initialize(Windows::Media::Core::MediaStreamSource ^ mss, Windows::Media::Core::VideoStreamDescriptor ^ videoDesc)
	{
		Microsoft::WRL::ComPtr<IMFDXGIDeviceManagerSource> spDeviceManagerSource;

		HRESULT hr = reinterpret_cast<IInspectable*>(mss)->QueryInterface(spDeviceManagerSource.GetAddressOf());

		if (_spDeviceManager == nullptr)
		{
			if (SUCCEEDED(hr))
			{
				hr = spDeviceManagerSource->GetManager(_spDeviceManager.ReleaseAndGetAddressOf());
				if (FAILED(hr))
				{
					UINT uiToken = 0;
					hr = MFCreateDXGIDeviceManager(&uiToken, _spDeviceManager.ReleaseAndGetAddressOf());
					if (SUCCEEDED(hr))
					{
						Microsoft::WRL::ComPtr<ID3D11Device> spDevice;
						D3D_FEATURE_LEVEL maxSupportedLevelByDevice = D3D_FEATURE_LEVEL_9_1;
						D3D_FEATURE_LEVEL rgFeatureLevels[] = {
							D3D_FEATURE_LEVEL_11_1,
							D3D_FEATURE_LEVEL_11_0,
							D3D_FEATURE_LEVEL_10_1,
							D3D_FEATURE_LEVEL_10_0,
							D3D_FEATURE_LEVEL_9_3,
							D3D_FEATURE_LEVEL_9_2,
							D3D_FEATURE_LEVEL_9_1
						};

						hr = D3D11CreateDevice(nullptr,
							D3D_DRIVER_TYPE_WARP,
							nullptr,
							D3D11_CREATE_DEVICE_BGRA_SUPPORT,
							rgFeatureLevels,
							ARRAYSIZE(rgFeatureLevels),
							D3D11_SDK_VERSION,
							spDevice.ReleaseAndGetAddressOf(),
							&maxSupportedLevelByDevice,
							nullptr);

						if (SUCCEEDED(hr))
						{
							hr = _spDeviceManager->ResetDevice(spDevice.Get(), uiToken);
						}
					}
				}
			}

			if (SUCCEEDED(hr))
			{
				hr = _spDeviceManager->OpenDeviceHandle(&_hDevice);
			}
		}

		if (SUCCEEDED(hr) && _spSampleAllocator == nullptr)
		{
			Microsoft::WRL::ComPtr<IMFMediaType> spVideoMT;
			if (SUCCEEDED(hr))
			{
				hr = ConvertPropertiesToMediaType(videoDesc->EncodingProperties, spVideoMT.GetAddressOf());
			}

			if (SUCCEEDED(hr))
			{
				hr = MFCreateVideoSampleAllocatorEx(IID_PPV_ARGS(_spSampleAllocator.ReleaseAndGetAddressOf()));
			}

			if (SUCCEEDED(hr))
			{
				hr = _spSampleAllocator->SetDirectXManager(_spDeviceManager.Get());
			}

			if (SUCCEEDED(hr))
			{
				hr = _spSampleAllocator->InitializeSampleAllocator(60, spVideoMT.Get());
			}
		}

		// We reset each time we are initialized
		_ulVideoTimestamp = 0;

		if (FAILED(hr))
		{
			throw Platform::Exception::CreateException(hr, L"Unable to initialize resources for the sample generator.");
		}
	}

	void SampleMaker::GenerateSample(Windows::Media::Core::MediaStreamSourceSampleRequest ^ request)
	{
		Microsoft::WRL::ComPtr<IMFMediaStreamSourceSampleRequest> spRequest;
		Windows::Media::MediaProperties::VideoEncodingProperties^ spEncodingProperties;
		HRESULT hr = (request != nullptr) ? S_OK : E_POINTER;
		UINT32 ui32Height = 0;
		UINT32 ui32Width = 0;
		ULONGLONG ulTimeSpan = 0;

		if (SUCCEEDED(hr))
		{
			hr = reinterpret_cast<IInspectable*>(request)->QueryInterface(spRequest.ReleaseAndGetAddressOf());
		}

		if (SUCCEEDED(hr))
		{
			Windows::Media::Core::VideoStreamDescriptor^ spVideoStreamDescriptor;
			spVideoStreamDescriptor = dynamic_cast<Windows::Media::Core::VideoStreamDescriptor^>(request->StreamDescriptor);
			if (spVideoStreamDescriptor != nullptr)
			{
				spEncodingProperties = spVideoStreamDescriptor->EncodingProperties;
			}
			else
			{
				throw Platform::Exception::CreateException(E_INVALIDARG, L"Media Request is not for an video sample.");
			}
		}

		if (SUCCEEDED(hr))
		{
			ui32Height = spEncodingProperties->Height;
		}

		if (SUCCEEDED(hr))
		{
			ui32Width = spEncodingProperties->Width;
		}

		if (SUCCEEDED(hr))
		{
			Windows::Media::MediaProperties::MediaRatio^ spRatio = spEncodingProperties->FrameRate;
			if (SUCCEEDED(hr))
			{
				UINT32 ui32Numerator = spRatio->Numerator;
				UINT32 ui32Denominator = spRatio->Denominator;
				if (ui32Numerator != 0)
				{
					ulTimeSpan = ((ULONGLONG)ui32Denominator) * 1000000 / ui32Numerator;
				}
				else
				{
					hr = E_INVALIDARG;
				}
			}
		}

		if (SUCCEEDED(hr))
		{
			Microsoft::WRL::ComPtr<IMFMediaBuffer> spBuffer;
			Microsoft::WRL::ComPtr<IMFSample> spSample;
			hr = _spSampleAllocator->AllocateSample(spSample.GetAddressOf());

			if (SUCCEEDED(hr))
			{
				hr = spSample->SetSampleDuration(ulTimeSpan);
			}

			if (SUCCEEDED(hr))
			{
				hr = spSample->SetSampleTime((LONGLONG)_ulVideoTimestamp);
			}

			if (SUCCEEDED(hr))
			{
				hr = spSample->GetBufferByIndex(0, spBuffer.GetAddressOf());
			}

			if (SUCCEEDED(hr))
			{
				IMFMediaBuffer * pBuffer = spBuffer.Get();

				byte * pBuf;
				DWORD bufMaxLen;
				DWORD bufCurrLen;

				hr = pBuffer->Lock(&pBuf, &bufMaxLen, &bufCurrLen);

				if (SUCCEEDED(hr))
				{
					byte rovingSq[] = { 0xA4, 0xa4, 0x12, 0xff, 
										0xA4, 0xa4, 0x12, 0xff,  
										0xA4, 0xa4, 0x12, 0xff,  
										0xA4, 0xa4, 0x12, 0xff, 
										0xA4, 0xa4, 0x12, 0xff,
										0xA4, 0xa4, 0x12, 0xff,
										0xA4, 0xa4, 0x12, 0xff,
										0xA4, 0xa4, 0x12, 0xff,
										0xA4, 0xa4, 0x12, 0xff,
										0xA4, 0xa4, 0x12, 0xff };
					
					int rovingSquareStart = (_iVideoFrameNumber * 40) % (ui32Width * ui32Height * 4);
					for (int i = 0; i < 10; i++)
					{
						if ((rovingSquareStart + i * ui32Width * 4) + 40 < (ui32Width * ui32Height * 4))
						{
							memcpy(pBuf + rovingSquareStart + i * ui32Width * 4, rovingSq, 40);
						}
					}

					hr = pBuffer->Unlock();
				}
			}

			if (SUCCEEDED(hr))
			{
				hr = spRequest->SetSample(spSample.Get());
			}

			if (SUCCEEDED(hr))
			{
				++_iVideoFrameNumber;
				_ulVideoTimestamp += ulTimeSpan;
			}
		}

		if (FAILED(hr))
		{
			throw Platform::Exception::CreateException(hr);
		}
	}

	HRESULT SampleMaker::AddAttribute(_In_ GUID guidKey, _In_ Windows::Foundation::IPropertyValue ^value, _In_ IMFAttributes *pAttr)
	{
		HRESULT hr = (value != nullptr && pAttr != nullptr) ? S_OK : E_INVALIDARG;
		if (SUCCEEDED(hr))
		{
			Windows::Foundation::PropertyType type = value->Type;
			switch (type)
			{
			case Windows::Foundation::PropertyType::UInt8Array:
			{
				Platform::Array<BYTE>^ arr;
				value->GetUInt8Array(&arr);

				hr = (pAttr->SetBlob(guidKey, arr->Data, arr->Length));
			}
			break;

			case Windows::Foundation::PropertyType::Double:
			{
				hr = (pAttr->SetDouble(guidKey, value->GetDouble()));
			}
			break;

			case Windows::Foundation::PropertyType::Guid:
			{
				hr = (pAttr->SetGUID(guidKey, value->GetGuid()));
			}
			break;

			case Windows::Foundation::PropertyType::String:
			{
				hr = (pAttr->SetString(guidKey, value->GetString()->Data()));
			}
			break;

			case Windows::Foundation::PropertyType::UInt32:
			{
				hr = (pAttr->SetUINT32(guidKey, value->GetUInt32()));
			}
			break;

			case Windows::Foundation::PropertyType::UInt64:
			{
				hr = (pAttr->SetUINT64(guidKey, value->GetUInt64()));
			}
			break;

			// ignore unknown values
			}
		}
		return hr;
	}

	HRESULT SampleMaker::ConvertPropertiesToMediaType(_In_ Windows::Media::MediaProperties::IMediaEncodingProperties ^mep, _Outptr_ IMFMediaType **ppMT)
	{
		HRESULT hr = (mep != nullptr && ppMT != nullptr) ? S_OK : E_INVALIDARG;
		Microsoft::WRL::ComPtr<IMFMediaType> spMT;
		if (SUCCEEDED(hr))
		{
			*ppMT = nullptr;
			hr = MFCreateMediaType(&spMT);
		}

		if (SUCCEEDED(hr))
		{
			auto it = mep->Properties->First();

			while (SUCCEEDED(hr) && it->HasCurrent)
			{
				auto currentValue = it->Current;
				hr = AddAttribute(currentValue->Key, safe_cast<Windows::Foundation::IPropertyValue^>(currentValue->Value), spMT.Get());
				it->MoveNext();
			}

			if (SUCCEEDED(hr))
			{
				GUID guiMajorType = safe_cast<Windows::Foundation::IPropertyValue^>(mep->Properties->Lookup(MF_MT_MAJOR_TYPE))->GetGuid();

				if (guiMajorType != MFMediaType_Video && guiMajorType != MFMediaType_Audio)
				{
					hr = E_UNEXPECTED;
				}
			}
		}

		if (SUCCEEDED(hr))
		{
			*ppMT = spMT.Detach();
		}

		return hr;
	}
}
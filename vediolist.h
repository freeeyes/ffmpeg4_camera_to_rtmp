#pragma once

#include <windows.h>
#include <vector>
#include <dshow.h>

#pragma comment(lib, "Strmiids.lib")

const int MAX_FRIENDLY_NAME_LENGTH = 128;
const int MAX_MONIKER_NAME_LENGTH = 256;

typedef struct _TDeviceName
{
	WCHAR FriendlyName[MAX_FRIENDLY_NAME_LENGTH];
	WCHAR MonikerName[MAX_MONIKER_NAME_LENGTH];
} TDeviceName;


static char* WCharToChar(WCHAR* s)
{
	int w_nlen = WideCharToMultiByte(CP_ACP, 0, s, -1, NULL, 0, NULL, false);
	char* ret = new char[w_nlen];
	memset(ret, 0, w_nlen);
	WideCharToMultiByte(CP_ACP, 0, s, -1, ret, w_nlen, NULL, false);
	return ret;

}

//REFGUID guidValue ： CLSID_AudioInputDeviceCategory（麦克风）；  CLSID_AudioRendererCategory（扬声器）； //CLSID_VideoInputDeviceCategory（摄像头）

static HRESULT GetAudioVideoInputDevices(std::vector<TDeviceName>& vectorDevices, REFGUID guidValue)
{
	TDeviceName name;
	HRESULT hr;

	// 初始化
	vectorDevices.clear();

	// 初始化COM
	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	if (FAILED(hr))
	{
		return hr;
	}

	// 创建系统设备枚举器实例
	ICreateDevEnum* pSysDevEnum = NULL;
	hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&pSysDevEnum);
	if (FAILED(hr))
	{
		CoUninitialize();
		return hr;
	}

	// 获取设备类枚举器

	IEnumMoniker* pEnumCat = NULL;
	hr = pSysDevEnum->CreateClassEnumerator(guidValue, &pEnumCat, 0);
	if (hr == S_OK)
	{
		// 枚举设备名称
		IMoniker* pMoniker = NULL;
		ULONG cFetched;
		while (pEnumCat->Next(1, &pMoniker, &cFetched) == S_OK)
		{
			IPropertyBag* pPropBag;
			hr = pMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag, (void**)&pPropBag);
			if (SUCCEEDED(hr))
			{
				// 获取设备友好名
				VARIANT varName;
				VariantInit(&varName);
				hr = pPropBag->Read(L"FriendlyName", &varName, NULL);
				if (SUCCEEDED(hr))
				{
					StringCchCopy(name.FriendlyName, MAX_FRIENDLY_NAME_LENGTH, varName.bstrVal);
					// 获取设备Moniker名
					LPOLESTR pOleDisplayName = reinterpret_cast<LPOLESTR>(CoTaskMemAlloc(MAX_MONIKER_NAME_LENGTH * 2));
					if (pOleDisplayName != NULL)
					{
						hr = pMoniker->GetDisplayName(NULL, NULL, &pOleDisplayName);

						if (SUCCEEDED(hr))
						{
							StringCchCopy(name.MonikerName, MAX_MONIKER_NAME_LENGTH, pOleDisplayName);
							vectorDevices.push_back(name);
						}

						CoTaskMemFree(pOleDisplayName);
					}
				}
				VariantClear(&varName);
				pPropBag->Release();
			}

			pMoniker->Release();
		} // End for While

		pEnumCat->Release();
	}

	pSysDevEnum->Release();
	CoUninitialize();
	return hr;
}

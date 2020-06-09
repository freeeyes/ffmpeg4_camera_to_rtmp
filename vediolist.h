#pragma once

#include <windows.h>
#include <vector>
#include <dshow.h>
#include <string>

#pragma comment(lib, "Strmiids.lib")

using namespace std;

const int MAX_FRIENDLY_NAME_LENGTH = 128;
const int MAX_MONIKER_NAME_LENGTH = 256;

typedef struct _TDeviceName
{
	WCHAR FriendlyName[MAX_FRIENDLY_NAME_LENGTH];
	WCHAR MonikerName[MAX_MONIKER_NAME_LENGTH];
} TDeviceName;

inline std::wstring Utf82Unicode(const std::string& utf8string)
{
	int widesize = ::MultiByteToWideChar(CP_UTF8, 0, utf8string.c_str(), -1, NULL, 0);
	if (widesize == ERROR_NO_UNICODE_TRANSLATION)
	{
		throw std::exception("Invalid UTF-8 sequence.");
	}
	if (widesize == 0)
	{
		throw std::exception("Error in conversion.");
	}

	std::vector<wchar_t> resultstring(widesize);

	int convresult = ::MultiByteToWideChar(CP_UTF8, 0, utf8string.c_str(), -1, &resultstring[0], widesize);

	if (convresult != widesize)
	{
		throw std::exception("La falla!");
	}

	return std::wstring(&resultstring[0]);
}


//unicode 转为 ascii  


inline std::string WideByte2Acsi(std::wstring& wstrcode)
{
	int asciisize = ::WideCharToMultiByte(CP_OEMCP, 0, wstrcode.c_str(), -1, NULL, 0, NULL, NULL);
	if (asciisize == ERROR_NO_UNICODE_TRANSLATION)
	{
		throw std::exception("Invalid UTF-8 sequence.");
	}
	if (asciisize == 0)
	{
		throw std::exception("Error in conversion.");
	}
	std::vector<char> resultstring(asciisize);
	int convresult = ::WideCharToMultiByte(CP_OEMCP, 0, wstrcode.c_str(), -1, &resultstring[0], asciisize, NULL, NULL);

	if (convresult != asciisize)
	{
		throw std::exception("La falla!");
	}

	return std::string(&resultstring[0]);
}

inline std::string UTF_82ASCII(std::string& strUtf8Code)
{
	std::string strRet("");
	//先把 utf8 转为 unicode  
	std::wstring wstr = Utf82Unicode(strUtf8Code);
	//最后把 unicode 转为 ascii  
	strRet = WideByte2Acsi(wstr);
	return strRet;
}

//ascii 转 Unicode  
inline std::wstring Acsi2WideByte(std::string& strascii)
{
	int widesize = MultiByteToWideChar(CP_ACP, 0, (char*)strascii.c_str(), -1, NULL, 0);
	if (widesize == ERROR_NO_UNICODE_TRANSLATION)
	{
		throw std::exception("Invalid UTF-8 sequence.");
	}
	if (widesize == 0)
	{
		throw std::exception("Error in conversion.");
	}
	std::vector<wchar_t> resultstring(widesize);
	int convresult = MultiByteToWideChar(CP_ACP, 0, (char*)strascii.c_str(), -1, &resultstring[0], widesize);


	if (convresult != widesize)
	{
		throw std::exception("La falla!");
	}

	return std::wstring(&resultstring[0]);
}


//Unicode 转 Utf8  
inline std::string Unicode2Utf8(const std::wstring& widestring)
{
	int utf8size = ::WideCharToMultiByte(CP_UTF8, 0, widestring.c_str(), -1, NULL, 0, NULL, NULL);
	if (utf8size == 0)
	{
		throw std::exception("Error in conversion.");
	}

	std::vector<char> resultstring(utf8size);

	int convresult = ::WideCharToMultiByte(CP_UTF8, 0, widestring.c_str(), -1, &resultstring[0], utf8size, NULL, NULL);

	if (convresult != utf8size)
	{
		throw std::exception("La falla!");
	}

	return std::string(&resultstring[0]);
}


//ascii 转 Utf8  
inline std::string ASCII2UTF_8(std::string& strAsciiCode)
{
	std::string strRet("");
	//先把 ascii 转为 unicode  
	std::wstring wstr = Acsi2WideByte(strAsciiCode);
	//最后把 unicode 转为 utf8  
	strRet = Unicode2Utf8(wstr);
	return strRet;
}


static char* WCharToChar(WCHAR* s)
{
	int w_nlen = WideCharToMultiByte(CP_ACP, 0, s, -1, NULL, 0, NULL, FALSE);
	char* ret = new char[w_nlen];
	memset(ret, 0, w_nlen);
	WideCharToMultiByte(CP_ACP, 0, s, -1, ret, w_nlen, NULL, FALSE);
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

#include "drivers/win_legacy/nt/os/os_windows_nt.h"

#include <oleauto.h>
#include <wbemidl.h>

class WindowsNTModelQuery {
	HRESULT _apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	IWbemLocator *_locator = nullptr;
	IWbemServices *_services = nullptr;
	IEnumWbemClassObject *_results = nullptr;
	IWbemClassObject *_computer = nullptr;

public:
	~WindowsNTModelQuery() {
		if (_computer) {
			_computer->Release();
		}
		if (_results) {
			_results->Release();
		}
		if (_services) {
			_services->Release();
		}
		if (_locator) {
			_locator->Release();
		}
		if (SUCCEEDED(_apartment)) {
			CoUninitialize();
		}
	}

	String read_model() {
		if (FAILED(_apartment) && _apartment != RPC_E_CHANGED_MODE) {
			return String();
		}
		if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<void **>(&_locator)))) {
			return String();
		}

		const BSTR local_namespace = SysAllocString(L"ROOT\\CIMV2");
		if (!local_namespace) {
			return String();
		}
		const HRESULT connected = _locator->ConnectServer(local_namespace, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &_services);
		SysFreeString(local_namespace);
		if (FAILED(connected) || FAILED(CoSetProxyBlanket(_services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE))) {
			return String();
		}

		const BSTR language = SysAllocString(L"WQL");
		const BSTR query = SysAllocString(L"SELECT Model FROM Win32_ComputerSystem");
		HRESULT queried = E_OUTOFMEMORY;
		if (language && query) {
			queried = _services->ExecQuery(language, query, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &_results);
		}
		SysFreeString(query);
		SysFreeString(language);
		if (FAILED(queried)) {
			return String();
		}

		ULONG returned = 0;
		if (FAILED(_results->Next(1000, 1, &_computer, &returned)) || !returned) {
			return String();
		}
		VARIANT value;
		VariantInit(&value);
		String model;
		if (SUCCEEDED(_computer->Get(L"Model", 0, &value, nullptr, nullptr)) && value.vt == VT_BSTR && value.bstrVal) {
			model = String(value.bstrVal, SysStringLen(value.bstrVal)).strip_edges();
		}
		VariantClear(&value);
		return model;
	}
};

String OS_WindowsNT::_read_hardware_registry_string(HKEY p_key, const wchar_t *p_value) {
	WCHAR text[256];
	DWORD length = sizeof(text) - sizeof(text[0]);
	DWORD type = 0;
	if (RegQueryValueExW(p_key, p_value, nullptr, &type, reinterpret_cast<BYTE *>(text), &length) != ERROR_SUCCESS ||
			type != REG_SZ || length % sizeof(text[0]) != 0) {
		return String();
	}
	// RegQueryValueExW does not guarantee a terminator for REG_SZ.
	text[length / sizeof(text[0])] = L'\0';
	return String(text).strip_edges();
}

String OS_WindowsNT::get_processor_name() const {
	HKEY processor;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_QUERY_VALUE, &processor) != ERROR_SUCCESS) {
		return String();
	}

	String name;
	const wchar_t *values[] = { L"ProcessorNameString", L"Identifier" };
	for (const wchar_t *value : values) {
		name = _read_hardware_registry_string(processor, value);
		if (!name.empty()) {
			break;
		}
	}
	RegCloseKey(processor);
	return name;
}

String OS_WindowsNT::get_model_name() const {
	HKEY bios;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", 0, KEY_QUERY_VALUE, &bios) == ERROR_SUCCESS) {
		const String model = _read_hardware_registry_string(bios, L"SystemProductName");
		const String board = _read_hardware_registry_string(bios, L"BaseBoardProduct");
		RegCloseKey(bios);
		if (!model.empty() && model.nocasecmp_to("System Product Name") != 0) {
			return model;
		}
		if (!board.empty() && board.nocasecmp_to("Base Board Product") != 0) {
			return board;
		}
	}

	WindowsNTModelQuery query;
	const String model = query.read_model();
	return model.empty() ? OS::get_model_name() : model;
}

String OS_WindowsNT::get_unique_id() const {
	HW_PROFILE_INFOW profile = {};
	if (!GetCurrentHwProfileW(&profile)) {
		return String();
	}
	return String(profile.szHwProfileGuid);
}

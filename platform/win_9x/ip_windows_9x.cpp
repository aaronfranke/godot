#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif // _WINSOCK_DEPRECATED_NO_WARNINGS

#include "ip_windows_9x.h"

#include "encoding_windows_9x.h"

#include <winsock2.h>
#include <ws2tcpip.h>

// Winsock declarations must precede the IP Helper types.
#include <iphlpapi.h>

void IPWindows9x::_resolve_hostname(List<IP_Address> &r_addresses, const String &p_hostname, Type p_type) const {
	if (p_type == TYPE_IPV6) {
		return;
	}
	const CharString hostname = p_hostname.utf8();
	const hostent *host = gethostbyname(hostname.get_data());
	if (!host || host->h_addrtype != AF_INET || host->h_length != 4 || !host->h_addr_list) {
		return;
	}
	for (char *const *address = host->h_addr_list; *address; ++address) {
		IP_Address ip;
		ip.set_ipv4(reinterpret_cast<const uint8_t *>(*address));
		if (!r_addresses.find(ip)) {
			r_addresses.push_back(ip);
		}
	}
}

bool IPWindows9x::_get_ip_helper_interfaces(Map<String, Interface_Info> *r_interfaces) const {
	typedef DWORD(WINAPI * GetAddressTable)(PMIB_IPADDRTABLE, PULONG, BOOL);
	typedef DWORD(WINAPI * GetInterface)(PMIB_IFROW);
	// Windows 98 supplies native interface indexes. Keep older installations on
	// the Winsock fallback without adding mandatory IP Helper function imports.
	static const HMODULE ip_helper = LoadLibraryA("iphlpapi.dll");
	static const auto get_table = ip_helper ? reinterpret_cast<GetAddressTable>(reinterpret_cast<void *>(GetProcAddress(ip_helper, "GetIpAddrTable"))) : nullptr;
	static const auto get_interface = ip_helper ? reinterpret_cast<GetInterface>(reinterpret_cast<void *>(GetProcAddress(ip_helper, "GetIfEntry"))) : nullptr;
	if (!get_table) {
		return false;
	}
	ULONG bytes = sizeof(MIB_IPADDRTABLE);
	Vector<uint8_t> data;
	DWORD result;
	do {
		if (data.resize(bytes) != OK) {
			return false;
		}
		result = get_table(reinterpret_cast<PMIB_IPADDRTABLE>(data.ptrw()), &bytes, FALSE);
	} while (result == ERROR_INSUFFICIENT_BUFFER && bytes > uint32_t(data.size()));
	if (result != NO_ERROR) {
		return false;
	}
	const auto *table = reinterpret_cast<const MIB_IPADDRTABLE *>(data.ptr());
	for (DWORD i = 0; i < table->dwNumEntries; ++i) {
		const MIB_IPADDRROW &native_address = table->table[i];
		if (native_address.dwAddr == INADDR_ANY) {
			continue;
		}
		Interface_Info info;
		info.index = String::num_uint64(native_address.dwIndex);
		info.name = "IPv4 " + info.index;
		MIB_IFROW native = {};
		native.dwIndex = native_address.dwIndex;
		if (get_interface && get_interface(&native) == NO_ERROR) {
			if (native.wszName[0]) {
				info.name = String(native.wszName);
			}
			DWORD description_length = 0;
			const DWORD maximum = MIN(native.dwDescrLen, DWORD(sizeof(native.bDescr)));
			while (description_length < maximum && native.bDescr[description_length]) {
				++description_length;
			}
			info.name_friendly = Windows9xEncoding::decode(reinterpret_cast<const char *>(native.bDescr), description_length);
		}
		if (info.name_friendly.empty()) {
			info.name_friendly = info.name;
		}
		IP_Address address;
		address.set_ipv4(reinterpret_cast<const uint8_t *>(&native_address.dwAddr));
		if (Map<String, Interface_Info>::Element *existing = r_interfaces->find(info.name)) {
			if (!existing->get().ip_addresses.find(address)) {
				existing->get().ip_addresses.push_back(address);
			}
		} else {
			info.ip_addresses.push_back(address);
			r_interfaces->insert(info.name, info);
		}
	}
	return true;
}

void IPWindows9x::get_local_interfaces(Map<String, Interface_Info> *r_interfaces) const {
	if (_get_ip_helper_interfaces(r_interfaces)) {
		return;
	}
	const SOCKET socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socket == INVALID_SOCKET) {
		return;
	}
	Vector<INTERFACE_INFO> interfaces;
	DWORD received = 0;
	int result = SOCKET_ERROR;
	for (int count = 8; count <= 256; count *= 2) {
		if (interfaces.resize(count) != OK) {
			break;
		}
		result = WSAIoctl(socket, SIO_GET_INTERFACE_LIST, nullptr, 0, interfaces.ptrw(), count * sizeof(INTERFACE_INFO), &received, nullptr, nullptr);
		if (result == 0 || WSAGetLastError() != WSAEFAULT) {
			break;
		}
	}
	closesocket(socket);
	if (result != 0 || received > DWORD(interfaces.size() * sizeof(INTERFACE_INFO))) {
		return;
	}
	for (uint32_t i = 0; i < received / sizeof(INTERFACE_INFO); ++i) {
		const INTERFACE_INFO &native = interfaces[i];
		if (!(native.iiFlags & IFF_UP) || native.iiAddress.Address.sa_family != AF_INET) {
			continue;
		}
		Interface_Info info;
		// SIO_GET_INTERFACE_LIST has no native index. Use a positive surrogate;
		// IPv4 multicast selects this interface by address, not by index.
		info.name = "IPv4 " + itos(i + 1);
		info.name_friendly = info.name;
		info.index = itos(i + 1);
		IP_Address address;
		address.set_ipv4(reinterpret_cast<const uint8_t *>(&native.iiAddress.AddressIn.sin_addr)); // codespell:ignore addressin
		info.ip_addresses.push_back(address);
		r_interfaces->insert(info.name, info);
	}
}

IP *IPWindows9x::_create_windows_9x() {
	return memnew(IPWindows9x);
}

void IPWindows9x::make_default() {
	_create = _create_windows_9x;
}

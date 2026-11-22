#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
// Windows 2000 needs the fallback to gethostbyname.
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif // _WINSOCK_DEPRECATED_NO_WARNINGS

#include "drivers/win_legacy/nt/ip_windows_nt.h"

#include <winsock2.h>
#include <ws2tcpip.h>

// Winsock definitions must precede the IP Helper declarations.
#include <iphlpapi.h>

IP_Address IPWindowsNT::_ip_from_sockaddr(const sockaddr *p_address, size_t p_length) {
	IP_Address ip;
	if (!p_address || p_length < sizeof(sockaddr)) {
		return ip;
	}
	if (p_address->sa_family == AF_INET && p_length >= sizeof(sockaddr_in)) {
		const auto *ipv4 = reinterpret_cast<const sockaddr_in *>(p_address);
		ip.set_ipv4(reinterpret_cast<const uint8_t *>(&ipv4->sin_addr));
	} else if (p_address->sa_family == AF_INET6 && p_length >= sizeof(sockaddr_in6)) {
		const auto *ipv6 = reinterpret_cast<const sockaddr_in6 *>(p_address);
		ip.set_ipv6(ipv6->sin6_addr.s6_addr);
	}
	return ip;
}

void IPWindowsNT::_resolve_hostname(List<IP_Address> &r_addresses, const String &p_hostname, Type p_type) const {
	using ResolveName = INT(WSAAPI *)(PCSTR, PCSTR, const ADDRINFOA *, PADDRINFOA *);
	using FreeAddresses = void(WSAAPI *)(PADDRINFOA);
	static const HMODULE winsock = GetModuleHandleW(L"ws2_32.dll");
	static const auto resolve_name = reinterpret_cast<ResolveName>(reinterpret_cast<void *>(GetProcAddress(winsock, "getaddrinfo")));
	static const auto free_addresses = reinterpret_cast<FreeAddresses>(reinterpret_cast<void *>(GetProcAddress(winsock, "freeaddrinfo")));

	const CharString hostname = p_hostname.utf8();
	if (resolve_name && free_addresses) {
		ADDRINFOA hints = {};
		hints.ai_socktype = SOCK_STREAM;
		if (p_type == TYPE_IPV4) {
			hints.ai_family = AF_INET;
		} else if (p_type == TYPE_IPV6) {
			hints.ai_family = AF_INET6;
		}
		PADDRINFOA results = nullptr;
		if (resolve_name(hostname.get_data(), nullptr, &hints, &results) == 0 && results) {
			for (const ADDRINFOA *entry = results; entry; entry = entry->ai_next) {
				const IP_Address ip = _ip_from_sockaddr(entry->ai_addr, entry->ai_addrlen);
				if (ip.is_valid() && !r_addresses.find(ip)) {
					r_addresses.push_back(ip);
				}
			}
			free_addresses(results);
		}
		return;
	}

	if (p_type == TYPE_IPV6) {
		return;
	}
	const hostent *host = gethostbyname(hostname.get_data());
	if (!host || !host->h_addr_list || host->h_addrtype != AF_INET || host->h_length != 4) {
		return;
	}
	// Winsock owns the thread-local result. Copy it before another lookup.
	for (char *const *entry = host->h_addr_list; *entry; ++entry) {
		IP_Address ip;
		ip.set_ipv4(reinterpret_cast<uint8_t *>(*entry));
		if (!r_addresses.find(ip)) {
			r_addresses.push_back(ip);
		}
	}
}

bool IPWindowsNT::_get_adapter_addresses(Map<String, Interface_Info> *r_interfaces) const {
	using GetAddresses = ULONG(WINAPI *)(ULONG, ULONG, PVOID, PIP_ADAPTER_ADDRESSES, PULONG);
	static const auto get_addresses = reinterpret_cast<GetAddresses>(reinterpret_cast<void *>(
			GetProcAddress(GetModuleHandleW(L"iphlpapi.dll"), "GetAdaptersAddresses")));
	if (!get_addresses) {
		return false;
	}

	ULONG bytes = 15 * 1024;
	Vector<uint8_t> data;
	ULONG result;
	do {
		if (data.resize(bytes) != OK) {
			return false;
		}
		result = get_addresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
				nullptr, reinterpret_cast<PIP_ADAPTER_ADDRESSES>(data.ptrw()), &bytes);
	} while (result == ERROR_BUFFER_OVERFLOW && bytes > ULONG(data.size()));
	if (result != NO_ERROR) {
		return false;
	}

	for (const auto *adapter = reinterpret_cast<const IP_ADAPTER_ADDRESSES *>(data.ptr()); adapter; adapter = adapter->Next) {
		Interface_Info info;
		info.name = adapter->AdapterName;
		info.name_friendly = String(adapter->FriendlyName);
		info.index = String::num_uint64(adapter->IfIndex ? adapter->IfIndex : adapter->Ipv6IfIndex);
		for (const auto *unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
			const auto &address = unicast->Address;
			if (address.iSockaddrLength > 0) {
				const IP_Address ip = _ip_from_sockaddr(address.lpSockaddr, address.iSockaddrLength);
				if (ip.is_valid()) {
					info.ip_addresses.push_back(ip);
				}
			}
		}
		if (!info.ip_addresses.empty()) {
			r_interfaces->insert(info.name, info);
		}
	}
	return true;
}

void IPWindowsNT::get_local_interfaces(Map<String, Interface_Info> *r_interfaces) const {
	if (_get_adapter_addresses(r_interfaces)) {
		return;
	}

	// Windows 2000 exposes the IPv4 address table, including loopback.
	ULONG bytes = sizeof(MIB_IPADDRTABLE);
	Vector<uint8_t> data;
	DWORD result;
	do {
		if (data.resize(bytes) != OK) {
			return;
		}
		result = GetIpAddrTable(reinterpret_cast<PMIB_IPADDRTABLE>(data.ptrw()), &bytes, FALSE);
	} while (result == ERROR_INSUFFICIENT_BUFFER && bytes > ULONG(data.size()));
	if (result != NO_ERROR) {
		return;
	}

	const auto *table = reinterpret_cast<const MIB_IPADDRTABLE *>(data.ptr());
	for (DWORD i = 0; i < table->dwNumEntries; ++i) {
		const MIB_IPADDRROW &address = table->table[i];
		if (address.dwAddr == INADDR_ANY) {
			continue;
		}
		IP_Address ip;
		ip.set_ipv4(reinterpret_cast<const uint8_t *>(&address.dwAddr));
		Interface_Info info;
		info.index = String::num_uint64(address.dwIndex);
		MIB_IFROW interface_row = {};
		interface_row.dwIndex = address.dwIndex;
		if (GetIfEntry(&interface_row) == NO_ERROR) {
			info.name = String(interface_row.wszName, MAX_INTERFACE_NAME_LEN);
		}
		if (info.name.empty()) {
			info.name = info.index;
		}
		if (auto *existing = r_interfaces->find(info.name)) {
			existing->get().ip_addresses.push_back(ip);
		} else {
			info.name_friendly = info.name;
			info.ip_addresses.push_back(ip);
			r_interfaces->insert(info.name, info);
		}
	}
}

void IPWindowsNT::make_default() {
	_create = []() -> IP * { return memnew(IPWindowsNT); };
}

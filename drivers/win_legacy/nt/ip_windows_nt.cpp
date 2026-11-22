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
	const CharString hostname = p_hostname.utf8();
	ADDRINFOA hints = {};
	hints.ai_socktype = SOCK_STREAM;
	if (p_type == TYPE_IPV4) {
		hints.ai_family = AF_INET;
	} else if (p_type == TYPE_IPV6) {
		hints.ai_family = AF_INET6;
	}
	PADDRINFOA results = nullptr;
	if (getaddrinfo(hostname.get_data(), nullptr, &hints, &results) == 0 && results) {
		for (const ADDRINFOA *entry = results; entry; entry = entry->ai_next) {
			const IP_Address ip = _ip_from_sockaddr(entry->ai_addr, entry->ai_addrlen);
			if (ip.is_valid() && !r_addresses.find(ip)) {
				r_addresses.push_back(ip);
			}
		}
		freeaddrinfo(results);
	}
}

bool IPWindowsNT::_get_adapter_addresses(Map<String, Interface_Info> *r_interfaces) const {
	ULONG bytes = 15 * 1024;
	Vector<uint8_t> data;
	ULONG result;
	do {
		if (data.resize(bytes) != OK) {
			return false;
		}
		result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
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
	_get_adapter_addresses(r_interfaces);
}

void IPWindowsNT::make_default() {
	_create = []() -> IP * { return memnew(IPWindowsNT); };
}

#pragma once

#include "core/io/ip.h"

struct sockaddr;

class IPWindowsNT : public IP {
	GDSOFTCLASS(IPWindowsNT, IP);

	static IPAddress _ip_from_sockaddr(const sockaddr *p_address, size_t p_length);
	bool _get_adapter_addresses(HashMap<String, Interface_Info> *r_interfaces) const;
	void _resolve_hostname(List<IPAddress> &r_addresses, const String &p_hostname, Type p_type) const override;

public:
	static void make_default();
	void get_local_interfaces(HashMap<String, Interface_Info> *r_interfaces) const override;
};

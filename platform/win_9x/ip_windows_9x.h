#pragma once

#include "core/io/ip.h"

class IPWindows9x : public IP {
	static IP *_create_windows_9x();
	bool _get_ip_helper_interfaces(Map<String, Interface_Info> *r_interfaces) const;

public:
	void _resolve_hostname(List<IP_Address> &r_addresses, const String &p_hostname, Type p_type) const override;
	void get_local_interfaces(Map<String, Interface_Info> *r_interfaces) const override;
	static void make_default();
};

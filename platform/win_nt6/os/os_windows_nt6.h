#pragma once

#include "drivers/win_legacy/nt/os/os_windows_nt.h"

class OS_WindowsNT6 : public OS_WindowsNT {
	void initialize() override;
	bool _check_internal_feature_support(const String &p_feature) override;
};

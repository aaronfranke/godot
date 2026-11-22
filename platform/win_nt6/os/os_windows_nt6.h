#pragma once

#include "drivers/win_legacy/nt/os/os_windows_nt.h"

class OS_WindowsNT6 : public OS_WindowsNT {
protected:
	bool _check_internal_feature_support(const String &p_feature) override;
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	DisplayServerWindowsNT *_create_display() override;
#endif // WIN_LEGACY_WINDOWED_ENABLED
};

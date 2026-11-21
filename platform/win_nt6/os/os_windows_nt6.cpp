#include "os_windows_nt6.h"

#ifdef WIN_LEGACY_WINDOWED_ENABLED
#include "platform/win_nt6/display/display_server_windows_nt6.h"
#endif // WIN_LEGACY_WINDOWED_ENABLED

void OS_WindowsNT6::initialize() {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	DisplayServerWindowsNT6::register_driver();
#endif // WIN_LEGACY_WINDOWED_ENABLED
	OS_WindowsNT::initialize();
}

bool OS_WindowsNT6::_check_internal_feature_support(const String &p_feature) {
	return p_feature == "win_nt6" || OS_WindowsNT::_check_internal_feature_support(p_feature);
}

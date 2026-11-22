#include "os_windows_nt5.h"

#ifdef WIN_LEGACY_WINDOWED_ENABLED
#include "platform/win_nt5/display/display_server_windows_nt5.h"
#endif // WIN_LEGACY_WINDOWED_ENABLED

bool OS_WindowsNT5::_check_internal_feature_support(const String &p_feature) {
	return p_feature == "win_nt5" || OS_WindowsNT::_check_internal_feature_support(p_feature);
}

#ifdef WIN_LEGACY_WINDOWED_ENABLED
DisplayServerWindowsNT *OS_WindowsNT5::_create_display() {
	return memnew(DisplayServerWindowsNT5);
}
#endif // WIN_LEGACY_WINDOWED_ENABLED

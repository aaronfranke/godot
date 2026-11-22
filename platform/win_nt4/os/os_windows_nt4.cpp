#include "os_windows_nt4.h"

#ifdef WIN_LEGACY_WINDOWED_ENABLED
#include "platform/win_nt4/display/display_server_windows_nt4.h"
#endif // WIN_LEGACY_WINDOWED_ENABLED

bool OS_WindowsNT4::_check_internal_feature_support(const String &p_feature) {
	return p_feature == "win_nt4" || OS_WindowsNT::_check_internal_feature_support(p_feature);
}

#ifdef WIN_LEGACY_WINDOWED_ENABLED
DisplayServerWindowsNT *OS_WindowsNT4::_create_display() {
	return memnew(DisplayServerWindowsNT4);
}
#endif // WIN_LEGACY_WINDOWED_ENABLED

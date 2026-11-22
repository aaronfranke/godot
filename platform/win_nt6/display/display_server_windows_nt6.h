#pragma once

#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// In 3.x the OS forwards its window/input API to this private display helper.
class DisplayServerWindowsNT6 : public DisplayServerWindowsNT {
};

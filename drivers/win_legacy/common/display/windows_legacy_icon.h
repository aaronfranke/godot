#pragma once

#include "core/math/vector2.h"
#include "core/reference.h"

#include <windows.h>

class Image;

// Native icon/cursor bitmap construction. Handle ownership remains in the display server.
class WindowsLegacyIcon {
public:
	static HICON create(const Ref<Image> &p_image, const Point2i *p_hotspot = nullptr);
};

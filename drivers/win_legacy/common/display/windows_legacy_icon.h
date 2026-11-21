#pragma once

#include "core/math/vector2i.h"
#include "core/object/ref_counted.h"

#include <windows.h>

class Image;

// Native icon/cursor bitmap construction. Handle ownership remains in the display server.
class WindowsLegacyIcon {
public:
	static HICON create(const Ref<Image> &p_image, const Point2i *p_hotspot = nullptr);
};

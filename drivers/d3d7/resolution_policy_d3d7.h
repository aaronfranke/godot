#ifndef D3D7_RESOLUTION_POLICY_H
#define D3D7_RESOLUTION_POLICY_H

#include "core/error_list.h"
#include "core/math/rect2.h"

class TestD3D7ResolutionPolicy;

class D3D7ResolutionPolicy {
	friend class TestD3D7ResolutionPolicy;

public:
	enum D3D7OversizeMode : uint8_t {
		D3D7_OVERSIZE_RESTRICTED,
		D3D7_OVERSIZE_STRETCH,
		D3D7_OVERSIZE_OVERRIDE,
		D3D7_OVERSIZE_FORCE_OVERRIDE,
		D3D7_OVERSIZE_MODE_MAX,
	};
	enum { UNPATCHED_LIMIT = 2048 };

private:
	D3D7OversizeMode _oversize_mode;
	uint32_t _limit;
	static void *_patched_module;
	static uint32_t _patched_limit;
	static volatile long _patch_lock;

	static bool _matches(const uint8_t *p_code, const uint8_t *p_pattern, const char *p_mask, size_t p_length);
	static bool _match_site(const uint8_t *p_code, size_t p_length, size_t p_offset, unsigned p_profile);
	static bool _find_sites(const uint8_t *p_image, size_t p_length, uint32_t r_sites[2]);
	static bool _readable(const uint8_t *p_base, size_t p_offset, size_t p_length);
	static Error _override_runtime(uint32_t p_limit);

public:
	static D3D7OversizeMode get_configured_mode();
	static Size2i get_screen_size(void *p_native_window);
	// Configure once, while the device is idle. Ordinary override uses the launch monitor.
	// Unknown/blocked runtime patches leave the bounded Stretch fallback available.
	Error configure(uint32_t p_texture_width, uint32_t p_texture_height, const Size2i &p_screen);
	void use_stretch_fallback();
	uint32_t get_limit() const { return _limit; }
	Size2i get_max_window_size() const;
	Size2i get_render_size(uint32_t p_width, uint32_t p_height, uint32_t p_max_width = 0, uint32_t p_max_height = 0) const;
	static Rect2i get_native_clip(const Rect2i &p_clip, const Size2i &p_logical, const Size2i &p_native);

	D3D7ResolutionPolicy(D3D7OversizeMode p_mode = D3D7_OVERSIZE_RESTRICTED);
};

#endif // D3D7_RESOLUTION_POLICY_H

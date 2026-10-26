#include "resolution_policy_d3d7.h"

#include "core/config/project_settings.h"
#include "drivers/fixed_function/graphics_device.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string.h>

void *D3D7ResolutionPolicy::_patched_module = nullptr;
uint32_t D3D7ResolutionPolicy::_patched_limit = 0;
volatile long D3D7ResolutionPolicy::_patch_lock = 0;

D3D7ResolutionPolicy::D3D7ResolutionPolicy(D3D7OversizeMode p_mode) :
		_oversize_mode(p_mode), _limit(UNPATCHED_LIMIT) {}

D3D7ResolutionPolicy::D3D7OversizeMode D3D7ResolutionPolicy::get_configured_mode() {
	if (!ProjectSettings::get_singleton()) {
		return D3D7_OVERSIZE_RESTRICTED;
	}
	const char *setting = "rendering/fixed_function/d3d7/oversize_mode";
	const int mode = GLOBAL_DEF_RST(PropertyInfo(Variant::INT, setting, PROPERTY_HINT_ENUM, "Restricted,Stretch,Override,Force Override"), D3D7_OVERSIZE_RESTRICTED);
	return mode >= 0 && mode < D3D7_OVERSIZE_MODE_MAX ? D3D7OversizeMode(mode) : D3D7_OVERSIZE_RESTRICTED;
}

Size2i D3D7ResolutionPolicy::get_screen_size(void *p_native_window) {
	// Multiple-monitor APIs are optional on the Windows 95 stretch target.
	HMODULE user32 = GetModuleHandleA("user32.dll");
	typedef HMONITOR(WINAPI * MonitorFromWindowFunction)(HWND, DWORD);
	typedef BOOL(WINAPI * GetMonitorInfoFunction)(HMONITOR, MONITORINFO *);
	MonitorFromWindowFunction monitor_from_window = reinterpret_cast<MonitorFromWindowFunction>(reinterpret_cast<void *>(GetProcAddress(user32, "MonitorFromWindow")));
	GetMonitorInfoFunction get_monitor_info = reinterpret_cast<GetMonitorInfoFunction>(reinterpret_cast<void *>(GetProcAddress(user32, "GetMonitorInfoA")));
	MONITORINFO monitor = {};
	monitor.cbSize = sizeof(monitor);
	if (monitor_from_window && get_monitor_info && get_monitor_info(monitor_from_window(static_cast<HWND>(p_native_window), MONITOR_DEFAULTTONEAREST), &monitor)) {
		return Size2i(monitor.rcMonitor.right - monitor.rcMonitor.left, monitor.rcMonitor.bottom - monitor.rcMonitor.top);
	}
	return Size2i(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
}

Error D3D7ResolutionPolicy::configure(uint32_t p_texture_width, uint32_t p_texture_height, const Size2i &p_screen) {
	_limit = UNPATCHED_LIMIT;
	if (_oversize_mode != D3D7_OVERSIZE_OVERRIDE && _oversize_mode != D3D7_OVERSIZE_FORCE_OVERRIDE) {
		return OK;
	}
	uint32_t limit = MIN(p_texture_width, p_texture_height);
	if (_oversize_mode == D3D7_OVERSIZE_OVERRIDE) {
		limit = MIN(limit, uint32_t(MAX(0, MAX(p_screen.x, p_screen.y))));
	}
	if (limit <= UNPATCHED_LIMIT) {
		return OK;
	}
	const Error error = _override_runtime(limit);
	if (error == OK) {
		_limit = limit;
		print_line("D3D7: runtime render-target limit overridden to " + itos(limit) + " pixels.");
	} else if (error != ERR_BUG) {
		WARN_PRINT("D3D7: resolution override unavailable; oversized render targets use proportional Stretch.");
	}
	// A failed rollback/protection restoration cannot safely continue rendering.
	return error == ERR_BUG ? error : OK;
}

Size2i D3D7ResolutionPolicy::get_max_window_size() const {
	return _oversize_mode == D3D7_OVERSIZE_RESTRICTED ? Size2i(UNPATCHED_LIMIT, UNPATCHED_LIMIT) : Size2i();
}

void D3D7ResolutionPolicy::use_stretch_fallback() {
	_limit = UNPATCHED_LIMIT;
	WARN_PRINT("D3D7: large native target creation failed; using proportional Stretch.");
}

Size2i D3D7ResolutionPolicy::get_render_size(uint32_t p_width, uint32_t p_height, uint32_t p_max_width, uint32_t p_max_height) const {
	if (!p_width || !p_height || p_width > INT32_MAX || p_height > INT32_MAX) {
		return Size2i();
	}
	const uint32_t max_width = p_max_width ? MIN(p_max_width, _limit) : _limit;
	const uint32_t max_height = p_max_height ? MIN(p_max_height, _limit) : _limit;
	if (p_width <= max_width && p_height <= max_height) {
		return Size2i(p_width, p_height);
	}
	// Choose the tighter axis with integer cross-products; round down to stay bounded.
	if (uint64_t(max_width) * p_height <= uint64_t(max_height) * p_width) {
		return Size2i(max_width, MAX(uint64_t(1), uint64_t(p_height) * max_width / p_width));
	}
	return Size2i(MAX(uint64_t(1), uint64_t(p_width) * max_height / p_height), max_height);
}

Rect2i D3D7ResolutionPolicy::get_native_clip(const Rect2i &p_clip, const Size2i &p_logical, const Size2i &p_native) {
	return FixedFunctionGraphicsDevice::get_texture_native_clip(p_clip, p_logical, p_native);
}

bool D3D7ResolutionPolicy::_matches(const uint8_t *p_code, const uint8_t *p_pattern, const char *p_mask, size_t p_length) {
	if (strlen(p_mask) != p_length) {
		return false;
	}
	for (size_t i = 0; i < p_length; i++) {
		if (p_mask[i] != '?' && p_code[i] != p_pattern[i]) {
			return false;
		}
	}
	return true;
}

bool D3D7ResolutionPolicy::_match_site(const uint8_t *p_code, size_t p_length, size_t p_offset, unsigned p_profile) {
	// Reviewed x86 layouts: CreateDevice (DDSURFACEDESC) and SetRenderTarget
	// (DDSURFACEDESC2). RVAs and branch/call destinations may move independently.
	static const uint8_t DESC0[] = { 0xc7, 0x45, 0x84, 0x6c, 0, 0, 0, 0x8b, 0x70, 0x58, 0x8d, 0x45, 0x84, 0x50, 0xff, 0xb5, 0x70, 0xff, 0xff, 0xff, 0x8b, 0xce, 0xff, 0x15, 0, 0, 0, 0, 0xff, 0xd6, 0x85, 0xc0, 0x0f, 0x85, 0, 0, 0, 0 };
	static const uint8_t DESC1[] = { 0xc7, 0x85, 0x38, 0xff, 0xff, 0xff, 0x7c, 0, 0, 0, 0x8b, 0x70, 0x58, 0x8d, 0x85, 0x38, 0xff, 0xff, 0xff, 0x50, 0x53, 0x8b, 0xce, 0xff, 0x15, 0, 0, 0, 0, 0xff, 0xd6, 0x8b, 0xf0, 0x85, 0xf6, 0x0f, 0x85, 0, 0, 0, 0 };
	static const uint8_t FLAGS0[] = { 0xf7, 0x45, 0xec, 0, 0x20, 0, 0, 0x0f, 0x84, 0, 0, 0, 0, 0xf7, 0x45, 0xec, 0, 0, 2, 0, 0x0f, 0x85, 0, 0, 0, 0 };
	static const uint8_t FLAGS1[] = { 0xf7, 0x45, 0xa0, 0, 0x20, 0, 0, 0x0f, 0x84, 0, 0, 0, 0, 0x8b, 0x4d, 0x8c, 0xe8, 0, 0, 0, 0, 0x8b, 0x8f, 0xd0, 0x22, 0, 0, 0x85, 0x81, 0xa0, 0, 0, 0, 0x0f, 0x84, 0, 0, 0, 0 };
	const size_t prefix = p_profile == 0 ? sizeof(DESC0) + sizeof(FLAGS0) : sizeof(DESC1) + sizeof(FLAGS1);
	const size_t comparison = p_profile == 0 ? 3 : 6;
	const size_t span = 5 + 2 * (comparison + 6);
	if (p_profile > 1 || p_offset < prefix || p_offset > p_length || p_length - p_offset < span + 2) {
		return false;
	}
	const uint8_t *site = p_code + p_offset;
	if (p_profile == 0) {
		if (!_matches(site - prefix, DESC0, "xxxxxxxxxxxxxxxxxxxxxxxx????xxxxxx????", sizeof(DESC0)) ||
				!_matches(site - sizeof(FLAGS0), FLAGS0, "xxxxxxxxx????xxxxxxxxx????", sizeof(FLAGS0))) {
			return false;
		}
	} else if (!_matches(site - prefix, DESC1, "xxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxx????", sizeof(DESC1)) ||
			!_matches(site - sizeof(FLAGS1), FLAGS1, "xxxxxxxxx????xxxx????xxxxxxxxxxxxxx????", sizeof(FLAGS1))) {
		return false;
	}
	if (site[0] != 0xb8 || site[1] != 0 || site[2] != 8 || site[3] != 0 || site[4] != 0) {
		return false;
	}
	const uint8_t cmp0[] = { 0x39, 0x45, 0x90 };
	const uint8_t cmp1[] = { 0x39, 0x85, 0x44, 0xff, 0xff, 0xff };
	const uint8_t *first = site + 5;
	const uint8_t *second = first + comparison + 6;
	if (memcmp(first, p_profile == 0 ? cmp0 : cmp1, comparison) || second[0] != first[0] || second[1] != first[1]) {
		return false;
	}
	int32_t first_field, second_field;
	if (p_profile == 0) {
		first_field = int8_t(first[2]);
		second_field = int8_t(second[2]);
	} else {
		memcpy(&first_field, first + 2, 4);
		memcpy(&second_field, second + 2, 4);
	}
	if (int64_t(first_field) != int64_t(second_field) + 4) {
		return false;
	}
	const uint8_t *jump0 = first + comparison;
	const uint8_t *jump1 = second + comparison;
	if (jump0[0] != 0x0f || jump0[1] != 0x87 || jump1[0] != 0x0f || jump1[1] != 0x87) {
		return false;
	}
	int32_t displacement0, displacement1;
	memcpy(&displacement0, jump0 + 2, 4);
	memcpy(&displacement1, jump1 + 2, 4);
	const int64_t failure = int64_t(jump0 - p_code) + 6 + displacement0;
	if (failure != int64_t(jump1 - p_code) + 6 + displacement1 || failure < int64_t(p_offset + span) || failure > int64_t(p_length) - 12) {
		return false;
	}
	const uint8_t error0[] = { 0xb8, 0x82, 0, 0x76, 0x88, 0xeb, 5, 0xb8, 0x64, 0, 0x76, 0x88 };
	const uint8_t error1[] = { 0xbe, 0x57, 0, 7, 0x80, 0xeb, 0, 0xbe, 0x64, 0, 0x76, 0x88 };
	if (!_matches(p_code + failure, p_profile == 0 ? error0 : error1, p_profile == 0 ? "xxxxxxxxxxxx" : "xxxxxx?xxxxx", 12)) {
		return false;
	}
	// Both surface-capability rejection branches must enter the adjacent bad-caps block.
	const size_t flags = p_offset - (p_profile == 0 ? sizeof(FLAGS0) : sizeof(FLAGS1));
	const size_t branches[] = { flags + 7, p_offset - 6 };
	for (unsigned i = 0; i < 2; i++) {
		int32_t displacement;
		memcpy(&displacement, p_code + branches[i] + 2, 4);
		if (int64_t(branches[i]) + 6 + displacement != failure + 7) {
			return false;
		}
	}
	return true;
}

bool D3D7ResolutionPolicy::_find_sites(const uint8_t *p_image, size_t p_length, uint32_t r_sites[2]) {
	if (p_length < sizeof(IMAGE_DOS_HEADER)) {
		return false;
	}
	const IMAGE_DOS_HEADER *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(p_image);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || size_t(dos->e_lfanew) > p_length || p_length - dos->e_lfanew < sizeof(IMAGE_NT_HEADERS32)) {
		return false;
	}
	const IMAGE_NT_HEADERS32 *nt = reinterpret_cast<const IMAGE_NT_HEADERS32 *>(p_image + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
			nt->FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER32) || nt->OptionalHeader.SizeOfImage != p_length) {
		return false;
	}
	const size_t table = dos->e_lfanew + sizeof(IMAGE_NT_HEADERS32);
	if (nt->FileHeader.NumberOfSections > (p_length - table) / sizeof(IMAGE_SECTION_HEADER)) {
		return false;
	}
	unsigned count[2] = {};
	const IMAGE_SECTION_HEADER *sections = reinterpret_cast<const IMAGE_SECTION_HEADER *>(p_image + table);
	for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; i++) {
		const IMAGE_SECTION_HEADER &section = sections[i];
		if (section.VirtualAddress > p_length || section.Misc.VirtualSize > p_length - section.VirtualAddress) {
			return false;
		}
		if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE)) {
			continue;
		}
		const uint8_t *code = p_image + section.VirtualAddress;
		for (size_t offset = 0; offset < section.Misc.VirtualSize; offset++) {
			if (code[offset] != 0xb8) {
				continue;
			}
			for (unsigned profile = 0; profile < 2; profile++) {
				if (_match_site(code, section.Misc.VirtualSize, offset, profile)) {
					r_sites[profile] = section.VirtualAddress + offset + 1;
					count[profile]++;
				}
			}
		}
	}
	return count[0] == 1 && count[1] == 1;
}

bool D3D7ResolutionPolicy::_readable(const uint8_t *p_base, size_t p_offset, size_t p_length) {
	if (p_offset > UINTPTR_MAX - uintptr_t(p_base) || p_length > UINTPTR_MAX - uintptr_t(p_base) - p_offset) {
		return false;
	}
	uintptr_t current = uintptr_t(p_base) + p_offset;
	const uintptr_t end = current + p_length;
	while (current < end) {
		MEMORY_BASIC_INFORMATION memory;
		if (!VirtualQuery(reinterpret_cast<void *>(current), &memory, sizeof(memory)) || memory.AllocationBase != p_base || memory.State != MEM_COMMIT ||
				(memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) || memory.RegionSize > UINTPTR_MAX - uintptr_t(memory.BaseAddress)) {
			return false;
		}
		const uintptr_t next = uintptr_t(memory.BaseAddress) + memory.RegionSize;
		if (next <= current) {
			return false;
		}
		current = next;
	}
	return true;
}

Error D3D7ResolutionPolicy::_override_runtime(uint32_t p_limit) {
	if (InterlockedCompareExchange(&_patch_lock, 1, 0)) {
		return ERR_BUSY;
	}
	// All exits release the inter-device lock; patching occurs before scene execution.
	struct PatchLock {
		volatile LONG *lock;
		~PatchLock() { InterlockedExchange(lock, 0); }
	} lock = { &_patch_lock };
	if (_patched_module) {
		if (!_patched_limit) {
			return ERR_BUG;
		}
		return p_limit <= _patched_limit ? OK : ERR_UNAVAILABLE;
	}
	// Win9x DLL sharing/isolation has not been qualified. Do not assume NT copy-on-write.
	typedef DWORD(WINAPI * GetVersionFunction)();
	GetVersionFunction get_version = reinterpret_cast<GetVersionFunction>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetVersion"));
	if (!get_version || (get_version() & 0x80000000u)) {
		return ERR_UNAVAILABLE;
	}
	HMODULE module = GetModuleHandleA("d3dim700.dll");
	const uint8_t *base = reinterpret_cast<const uint8_t *>(module);
	if (!module || !_readable(base, 0, sizeof(IMAGE_DOS_HEADER))) {
		return ERR_UNAVAILABLE;
	}
	const IMAGE_DOS_HEADER *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || !_readable(base, dos->e_lfanew, sizeof(IMAGE_NT_HEADERS32))) {
		return ERR_UNAVAILABLE;
	}
	const IMAGE_NT_HEADERS32 *nt = reinterpret_cast<const IMAGE_NT_HEADERS32 *>(base + dos->e_lfanew);
	const size_t table = size_t(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS32);
	const size_t table_size = size_t(nt->FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
	if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER32) ||
			nt->OptionalHeader.SizeOfImage < table || table_size > nt->OptionalHeader.SizeOfImage - table || !_readable(base, table, table_size)) {
		return ERR_UNAVAILABLE;
	}
	const IMAGE_SECTION_HEADER *sections = reinterpret_cast<const IMAGE_SECTION_HEADER *>(base + table);
	for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; i++) {
		if ((sections[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) && !_readable(base, sections[i].VirtualAddress, sections[i].Misc.VirtualSize)) {
			return ERR_UNAVAILABLE;
		}
	}
	uint32_t sites[2];
	if (!_find_sites(base, nt->OptionalHeader.SizeOfImage, sites)) {
		return ERR_UNAVAILABLE;
	}
	// Retain a loader reference: successful edits and our cached state last until process exit.
	char path[MAX_PATH];
	const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
	if (!length || length >= MAX_PATH) {
		return ERR_UNAVAILABLE;
	}
	HMODULE retained = LoadLibraryA(path);
	if (retained != module) {
		if (retained) {
			FreeLibrary(retained);
		}
		return ERR_UNAVAILABLE;
	}
	DWORD protections[2];
	unsigned writable = 0;
	for (; writable < 2; writable++) {
		if (!VirtualProtect(const_cast<uint8_t *>(base) + sites[writable], 4, PAGE_EXECUTE_READWRITE, &protections[writable])) {
			break;
		}
	}
	Error result = ERR_UNAVAILABLE;
	uint32_t verified_sites[2];
	// Revalidate after loader/protection operations, immediately before committing edits.
	if (writable == 2 && _find_sites(base, nt->OptionalHeader.SizeOfImage, verified_sites) && sites[0] == verified_sites[0] && sites[1] == verified_sites[1]) {
		for (unsigned i = 0; i < 2; i++) {
			memcpy(const_cast<uint8_t *>(base) + sites[i], &p_limit, 4);
		}
		if (FlushInstructionCache(GetCurrentProcess(), base, nt->OptionalHeader.SizeOfImage)) {
			result = OK;
		} else {
			const uint32_t original = UNPATCHED_LIMIT;
			for (unsigned i = 0; i < 2; i++) {
				memcpy(const_cast<uint8_t *>(base) + sites[i], &original, 4);
			}
			if (!FlushInstructionCache(GetCurrentProcess(), base, nt->OptionalHeader.SizeOfImage)) {
				result = ERR_BUG;
			}
		}
	}
	while (writable) {
		writable--;
		DWORD ignored;
		if (!VirtualProtect(const_cast<uint8_t *>(base) + sites[writable], 4, protections[writable], &ignored)) {
			result = ERR_BUG;
		}
	}
	if (result == OK || result == ERR_BUG) {
		_patched_module = retained;
		_patched_limit = result == OK ? p_limit : 0;
	} else {
		FreeLibrary(retained);
	}
	return result;
}

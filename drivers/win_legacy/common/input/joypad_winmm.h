#pragma once

#include "core/input/input_enums.h"
#include "core/templates/bit_field.h"

#include <windows.h>

#include <mmsystem.h>

class JoypadWinMM {
	typedef JOYCAPSW WinMMCapabilities;

	enum { SLOT_COUNT = 16 };
	static constexpr uint64_t SCAN_INTERVAL_USEC = 500000;

	struct Slot {
		WinMMCapabilities caps = {};
		DWORD poll_flags = 0;
		int device_id = -1;
	};

	Slot _slots[SLOT_COUNT];
	uint64_t _next_scan = 0;

	static DWORD _poll_flags(const WinMMCapabilities &p_caps);
	static float _normalize_axis(DWORD p_value, UINT p_min, UINT p_max);
	static BitField<HatMask> _get_hat(DWORD p_pov);
	static void _submit(const Slot &p_slot, const JOYINFOEX *p_state);
	static void _disconnect(Slot &r_slot);

public:
	void process();
	void release_inputs();
	~JoypadWinMM();
};

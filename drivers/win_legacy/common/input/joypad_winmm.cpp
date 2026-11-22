#include "drivers/win_legacy/common/input/joypad_winmm.h"

// Godot includes.
#include "core/os/os.h"
#include "main/input_default.h"

DWORD JoypadWinMM::_poll_flags(const WinMMCapabilities &p_caps) {
	DWORD flags = JOY_RETURNX | JOY_RETURNY | JOY_RETURNBUTTONS;
	if (p_caps.wCaps & JOYCAPS_HASZ) {
		flags |= JOY_RETURNZ;
	}
	if (p_caps.wCaps & JOYCAPS_HASR) {
		flags |= JOY_RETURNR;
	}
	if (p_caps.wCaps & JOYCAPS_HASU) {
		flags |= JOY_RETURNU;
	}
	if (p_caps.wCaps & JOYCAPS_HASV) {
		flags |= JOY_RETURNV;
	}
	if (p_caps.wCaps & JOYCAPS_HASPOV) {
		flags |= (p_caps.wCaps & JOYCAPS_POVCTS) ? JOY_RETURNPOVCTS : JOY_RETURNPOV;
	}
	return flags;
}

float JoypadWinMM::_normalize_axis(DWORD p_value, UINT p_min, UINT p_max) {
	if (p_min >= p_max) {
		return 0.0f;
	}
	const double position = (double(p_value) - p_min) / (double(p_max) - p_min);
	return float(CLAMP(2.0 * position - 1.0, -1.0, 1.0));
}

int JoypadWinMM::_get_hat(DWORD p_pov) {
	if (p_pov >= 36000) {
		return InputDefault::HAT_MASK_CENTER;
	}
	constexpr int UP = InputDefault::HAT_MASK_UP;
	constexpr int RIGHT = InputDefault::HAT_MASK_RIGHT;
	constexpr int DOWN = InputDefault::HAT_MASK_DOWN;
	constexpr int LEFT = InputDefault::HAT_MASK_LEFT;
	constexpr int DIRECTIONS[] = { UP, UP | RIGHT, RIGHT, RIGHT | DOWN, DOWN, DOWN | LEFT, LEFT, LEFT | UP };
	return DIRECTIONS[((p_pov + 2250) / 4500) % 8];
}

void JoypadWinMM::_submit(InputDefault &p_input, const Slot &p_slot, const JOYINFOEX *p_state) {
	const WinMMCapabilities &caps = p_slot.caps;
	const JOYINFOEX neutral = {};
	const JOYINFOEX &state = p_state ? *p_state : neutral;
	const bool has_pov = caps.wCaps & JOYCAPS_HASPOV;
	for (UINT button = 0; button < MIN(caps.wNumButtons, 32u); ++button) {
		// On hat-equipped devices, leave the 3.x D-pad indices for hat events.
		const UINT index = has_pov && button >= UINT(JOY_DPAD_UP) ? button + 4 : button;
		p_input.joy_button(p_slot.device_id, index, (state.dwButtons & (DWORD(1) << button)) != 0);
	}
	if (has_pov) {
		p_input.joy_hat(p_slot.device_id, _get_hat(p_state ? state.dwPOV : JOY_POVCENTERED));
	}

	const DWORD flags[] = { JOY_RETURNX, JOY_RETURNY, JOY_RETURNZ, JOY_RETURNR, JOY_RETURNU, JOY_RETURNV };
	const UINT minimum[] = { caps.wXmin, caps.wYmin, caps.wZmin, caps.wRmin, caps.wUmin, caps.wVmin };
	const UINT maximum[] = { caps.wXmax, caps.wYmax, caps.wZmax, caps.wRmax, caps.wUmax, caps.wVmax };
	const DWORD values[] = { state.dwXpos, state.dwYpos, state.dwZpos, state.dwRpos, state.dwUpos, state.dwVpos };
	int axis = 0;
	for (int i = 0; i < 6; ++i) {
		if (p_slot.poll_flags & flags[i]) {
			const float value = p_state ? _normalize_axis(values[i], minimum[i], maximum[i]) : 0.0f;
			p_input.joy_axis(p_slot.device_id, axis++, value);
		}
	}
}

void JoypadWinMM::_disconnect(InputDefault &p_input, Slot &r_slot) {
	p_input.joy_connection_changed(r_slot.device_id, false, String());
	r_slot.device_id = -1;
}

void JoypadWinMM::process(InputDefault &p_input) {
	const uint64_t now = OS::get_singleton()->get_ticks_usec();
	const bool scan = now >= _next_scan;
	UINT available_slots = 0;
	if (scan) {
		available_slots = MIN(joyGetNumDevs(), UINT(SLOT_COUNT));
		_next_scan = now + SCAN_INTERVAL_USEC;
	}
	for (UINT i = 0; i < SLOT_COUNT; ++i) {
		Slot &slot = _slots[i];
		if (slot.device_id < 0) {
			if (!scan || i >= available_slots) {
				continue;
			}
			const MMRESULT result = joyGetDevCapsW(i, &slot.caps, sizeof(slot.caps));
			if (result != JOYERR_NOERROR) {
				continue;
			}
			slot.poll_flags = _poll_flags(slot.caps);
		}

		JOYINFOEX state = {};
		state.dwSize = sizeof(state);
		state.dwFlags = slot.poll_flags;
		state.dwPOV = JOY_POVCENTERED;
		if (joyGetPosEx(i, &state) != JOYERR_NOERROR) {
			if (slot.device_id >= 0) {
				// 3.x clears the raw state on disconnect. Neutral events also release mapped actions.
				_submit(p_input, slot, nullptr);
				_disconnect(p_input, slot);
			}
			continue;
		}
		if (slot.device_id < 0) {
			slot.device_id = p_input.get_unused_joy_id();
			if (slot.device_id < 0) {
				continue;
			}
			slot.caps.szPname[MAXPNAMELEN - 1] = 0;
			const String name(slot.caps.szPname);
			p_input.joy_connection_changed(slot.device_id, true, name.empty() ? String("WinMM joystick") : name);
		}
		if (_input_enabled) {
			_submit(p_input, slot, &state);
		}
	}
}

void JoypadWinMM::set_input_enabled(InputDefault &p_input, bool p_enable) {
	if (_input_enabled == p_enable) {
		return;
	}
	_input_enabled = p_enable;
	if (!p_enable) {
		for (const Slot &slot : _slots) {
			if (slot.device_id >= 0) {
				_submit(p_input, slot, nullptr);
			}
		}
	}
}

void JoypadWinMM::close(InputDefault &p_input) {
	for (Slot &slot : _slots) {
		if (slot.device_id >= 0) {
			_disconnect(p_input, slot);
		}
	}
}

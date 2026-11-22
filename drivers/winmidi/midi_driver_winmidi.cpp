/**************************************************************************/
/*  midi_driver_winmidi.cpp                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifdef WINMIDI_ENABLED

#include "midi_driver_winmidi.h"

#include "core/print_string.h"

LRESULT CALLBACK MIDIDriverWinMidi::_window_proc(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_lparam) {
	if (p_message == WM_NCCREATE) {
		const CREATESTRUCTW *create = reinterpret_cast<const CREATESTRUCTW *>(p_lparam);
		SetWindowLongPtrW(p_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
	} else if (p_message == MM_MIM_DATA) {
		MIDIDriverWinMidi *driver = reinterpret_cast<MIDIDriverWinMidi *>(GetWindowLongPtrW(p_window, GWLP_USERDATA));
		if (driver) {
			driver->_read(reinterpret_cast<HMIDIIN>(p_wparam), uint32_t(p_lparam));
		}
		return 0;
	}
	return DefWindowProcW(p_window, p_message, p_wparam, p_lparam);
}

void MIDIDriverWinMidi::_read(HMIDIIN p_handle, uint32_t p_message) {
	for (int i = 0; i < _devices.size(); i++) {
		if (_devices[i].handle == p_handle) {
			uint8_t data[] = { uint8_t(p_message), uint8_t(p_message >> 8), uint8_t(p_message >> 16) };
			receive_input_packet(0, data, sizeof(data), i);
			return;
		}
	}
}

Error MIDIDriverWinMidi::open() {
	if (_message_window) {
		return OK;
	}
	const UINT count = midiInGetNumDevs();
	if (!count) {
		return OK;
	}

	WNDCLASSW window_class = {};
	window_class.lpfnWndProc = _window_proc;
	window_class.hInstance = GetModuleHandleW(nullptr);
	window_class.lpszClassName = L"GodotWinMMMidiInput";
	ERR_FAIL_COND_V_MSG(!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS,
			ERR_CANT_CREATE, "Could not register the WinMM MIDI message window class.");
	// WinMM posts short MIDI messages to the same thread that dispatches Godot input.
	const HWND parent = HWND_MESSAGE;
	_message_window = CreateWindowExW(0, window_class.lpszClassName, nullptr, 0, 0, 0, 0, 0,
			parent, nullptr, window_class.hInstance, this);
	ERR_FAIL_NULL_V_MSG(_message_window, ERR_CANT_CREATE, "Could not create the WinMM MIDI message window.");

	for (UINT i = 0; i < count; i++) {
		MIDIINCAPSW caps = {};
		const bool has_name = midiInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR;
		const String name = has_name ? String(caps.szPname) : vformat("MIDI input %d", i);
		HMIDIIN input = nullptr;
		MMRESULT result = midiInOpen(&input, i, reinterpret_cast<DWORD_PTR>(_message_window), 0, CALLBACK_WINDOW);
		if (result == MMSYSERR_NOERROR) {
			result = midiInStart(input);
			if (result == MMSYSERR_NOERROR) {
				_devices.push_back({ name, input });
				continue;
			}
			midiInClose(input);
		}
		wchar_t error_text[256];
		const String reason = midiInGetErrorTextW(result, error_text, ARRAYSIZE(error_text)) == MMSYSERR_NOERROR ? String(error_text) : itos(result);
		ERR_PRINT(vformat("Could not start WinMM MIDI input \"%s\": %s", name, reason));
	}
	if (_devices.empty()) {
		close();
	}
	return OK;
}

PoolStringArray MIDIDriverWinMidi::get_connected_inputs() {
	PoolStringArray list;
	for (int i = 0; i < _devices.size(); i++) {
		list.push_back(_devices[i].name);
	}
	return list;
}

void MIDIDriverWinMidi::close() {
	if (!_message_window) {
		return;
	}
	SetWindowLongPtrW(_message_window, GWLP_USERDATA, 0);
	for (int i = 0; i < _devices.size(); i++) {
		midiInStop(_devices[i].handle);
		midiInClose(_devices[i].handle);
	}
	_devices.clear();
	DestroyWindow(_message_window);
	_message_window = nullptr;
}

void MIDIDriverWinMidi::process() {
	MSG message;
	while (_message_window && PeekMessageW(&message, _message_window, MM_MIM_OPEN, MM_MIM_LONGERROR, PM_REMOVE)) {
		if (message.message == WM_QUIT) {
			PostQuitMessage(int(message.wParam));
			break;
		}
		DispatchMessageW(&message);
	}
}

MIDIDriverWinMidi::~MIDIDriverWinMidi() {
	close();
}

#endif // WINMIDI_ENABLED

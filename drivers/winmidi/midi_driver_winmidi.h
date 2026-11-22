/**************************************************************************/
/*  midi_driver_winmidi.h                                                 */
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

#ifndef MIDI_DRIVER_WINMIDI_H
#define MIDI_DRIVER_WINMIDI_H

#ifdef WINMIDI_ENABLED

#include "core/os/midi_driver.h"
#include "core/vector.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <mmsystem.h>

class MIDIDriverWinMidi : public MIDIDriver {
	struct MIDIInputDevice {
		String name;
		HMIDIIN handle;
	};

	Vector<MIDIInputDevice> _devices;
	HWND _message_window = nullptr;

	static LRESULT CALLBACK _window_proc(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_lparam);
	void _read(HMIDIIN p_handle, uint32_t p_message);

public:
	Error open() override;
	void close() override;
	PoolStringArray get_connected_inputs() override;
	void process();

	~MIDIDriverWinMidi() override;
};

#endif // WINMIDI_ENABLED

#endif // MIDI_DRIVER_WINMIDI_H

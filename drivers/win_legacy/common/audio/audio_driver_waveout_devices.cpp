#include "drivers/win_legacy/common/audio/audio_driver_waveout.h"

#include "core/set.h"

#ifdef WIN_9X_ENABLED
#include "platform/win_9x/encoding_windows_9x.h"
#endif // WIN_9X_ENABLED

Vector<AudioDriverWaveOut::WaveDevice> AudioDriverWaveOut::_enumerate_devices(bool p_input) {
	Vector<WaveDevice> devices;
	devices.push_back({ "Default", WAVE_MAPPER });
	Set<String> used_names;
	used_names.insert("Default");
	const UINT count = p_input ? waveInGetNumDevs() : waveOutGetNumDevs();
	for (UINT id = 0; id < count; ++id) {
		String name;
		if (p_input) {
#ifdef WIN_9X_ENABLED
			WAVEINCAPSA caps = {};
			if (waveInGetDevCapsA(id, &caps, sizeof(caps)) != MMSYSERR_NOERROR) {
				continue;
			}
			name = Windows9xEncoding::decode(caps.szPname);
#else
			WAVEINCAPSW caps = {};
			if (waveInGetDevCapsW(id, &caps, sizeof(caps)) != MMSYSERR_NOERROR) {
				continue;
			}
			name = String(caps.szPname);
#endif // WIN_9X_ENABLED
		} else {
#ifdef WIN_9X_ENABLED
			WAVEOUTCAPSA caps = {};
			if (waveOutGetDevCapsA(id, &caps, sizeof(caps)) != MMSYSERR_NOERROR) {
				continue;
			}
			name = Windows9xEncoding::decode(caps.szPname);
#else
			WAVEOUTCAPSW caps = {};
			if (waveOutGetDevCapsW(id, &caps, sizeof(caps)) != MMSYSERR_NOERROR) {
				continue;
			}
			name = String(caps.szPname);
#endif // WIN_9X_ENABLED
		}
		if (name.empty()) {
			name = vformat("WinMM device %d", id + 1);
		}
		const String base_name = name;
		for (int duplicate = 2; used_names.has(name); ++duplicate) {
			name = vformat("%s (%d)", base_name, duplicate);
		}
		used_names.insert(name);
		devices.push_back({ name, id });
	}
	return devices;
}

Array AudioDriverWaveOut::_get_device_list(bool p_input) {
	Array names;
	const Vector<WaveDevice> devices = _enumerate_devices(p_input);
	for (int i = 0; i < devices.size(); ++i) {
		names.push_back(devices[i].name);
	}
	return names;
}

AudioDriverWaveOut::WaveDevice AudioDriverWaveOut::_find_device(const String &p_name, bool p_input) {
	if (p_name != "Default" && !p_name.empty()) {
		const Vector<WaveDevice> devices = _enumerate_devices(p_input);
		for (int i = 0; i < devices.size(); ++i) {
			if (devices[i].name == p_name) {
				return devices[i];
			}
		}
	}
	return { "Default", WAVE_MAPPER };
}

Array AudioDriverWaveOut::get_device_list() {
	return _get_device_list(false);
}

String AudioDriverWaveOut::get_device() {
	MutexLock lock(_mutex);
	return _output_device;
}

void AudioDriverWaveOut::set_device(String p_name) {
	MutexLock lock(_mutex);
	if (_event && !_stop.is_set()) {
		_requested_output_device = p_name;
		_output_device_pending = true;
		SetEvent(_event);
	}
}

Array AudioDriverWaveOut::capture_get_device_list() {
	return _get_device_list(true);
}

String AudioDriverWaveOut::capture_get_device() {
	MutexLock lock(_mutex);
	return _input_device;
}

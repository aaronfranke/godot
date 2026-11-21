#include "drivers/win_legacy/common/audio/audio_driver_waveout.h"

// Godot includes.
#include "core/string/print_string.h"

MMRESULT AudioDriverWaveOut::_open_capture_device(Capture &r_capture, UINT p_id, int &r_mix_rate, bool p_fixed_rate) {
	const int rates[] = { r_mix_rate, AudioDriverManager::DEFAULT_MIX_RATE, 48000, 22050, 11025, 8000 };
	const unsigned int rate_count = p_fixed_rate ? 1 : ARRAYSIZE(rates);
	for (unsigned int i = 0; i < rate_count; ++i) {
		const int rate = rates[i];
		if (i != 0 && rate == rates[0]) {
			continue;
		}
		for (int channels = CHANNELS; channels >= 1; --channels) {
			WAVEFORMATEX format = {};
			if (!_get_pcm_format(rate, channels, format)) {
				continue;
			}
			HWAVEIN device = nullptr;
			const MMRESULT result = waveInOpen(&device, p_id, &format, reinterpret_cast<DWORD_PTR>(_event), 0, CALLBACK_EVENT);
			if (result == MMSYSERR_NOERROR) {
				r_capture.device = device;
				r_capture.channels = channels;
				r_mix_rate = rate;
				return result;
			}
			if (result != WAVERR_BADFORMAT) {
				return result;
			}
		}
	}
	return WAVERR_BADFORMAT;
}

Error AudioDriverWaveOut::_start_capture(Capture *&r_capture, UINT p_id, int &r_mix_rate, bool p_fixed_rate) {
	r_capture = memnew(Capture);
	MMRESULT result = _open_capture_device(*r_capture, p_id, r_mix_rate, p_fixed_rate);
	if (result != MMSYSERR_NOERROR) {
		print_verbose(vformat("waveIn could not open the input device (error %d).", result));
		_stop_capture(r_capture);
		return result == MMSYSERR_ALLOCATED ? ERR_ALREADY_IN_USE : ERR_UNAVAILABLE;
	}

	const int frames_per_buffer = CLAMP(r_mix_rate / 100, 128, 16384);
	const int samples_per_buffer = frames_per_buffer * r_capture->channels;
	if (r_capture->samples.resize(samples_per_buffer * BUFFER_COUNT) != OK) {
		_stop_capture(r_capture);
		return ERR_OUT_OF_MEMORY;
	}
	int16_t *samples = r_capture->samples.ptrw();
	for (int i = 0; i < BUFFER_COUNT; ++i) {
		WAVEHDR &header = r_capture->headers[i];
		header.lpData = reinterpret_cast<LPSTR>(samples + i * samples_per_buffer);
		header.dwBufferLength = samples_per_buffer * sizeof(int16_t);
		result = waveInPrepareHeader(r_capture->device, &header, sizeof(header));
		if (result == MMSYSERR_NOERROR) {
			result = waveInAddBuffer(r_capture->device, &header, sizeof(header));
		}
		if (result != MMSYSERR_NOERROR) {
			print_verbose(vformat("waveIn could not queue a capture buffer (error %d).", result));
			_stop_capture(r_capture);
			return ERR_CANT_CREATE;
		}
	}

	result = waveInStart(r_capture->device);
	if (result != MMSYSERR_NOERROR) {
		print_verbose(vformat("waveIn could not start capture (error %d).", result));
		_stop_capture(r_capture);
		return ERR_CANT_CREATE;
	}
	r_capture->running = true;
	return OK;
}

Error AudioDriverWaveOut::input_start() {
	MutexLock lock(_mutex);
	if (!_playback || _stop.is_set()) {
		return ERR_UNAVAILABLE;
	}
	if (_capture && _capture->running) {
		return OK;
	}
	if (_stop_capture(_capture) != OK || _stop_capture(_retired_capture) != OK) {
		return ERR_BUSY;
	}
	const WaveDevice device = _find_device(_input_device, true);
	_input_device = device.name;
	int mix_rate = _mix_rate;
	const Error error = _start_capture(_capture, device.id, mix_rate, false);
	if (error != OK) {
		return error;
	}
	const int frames_per_buffer = _capture->headers[0].dwBufferLength / (sizeof(int16_t) * _capture->channels);
	input_buffer_init(frames_per_buffer * BUFFER_COUNT);
	if (input_buffer.is_empty()) {
		_stop_capture(_capture);
		return ERR_OUT_OF_MEMORY;
	}
	_input_mix_rate.set(mix_rate);
	return OK;
}

void AudioDriverWaveOut::set_input_device(const String &p_name) {
	MutexLock lock(_mutex);
	const WaveDevice device = _find_device(p_name, true);
	if (device.name == _input_device || _stop.is_set()) {
		return;
	}
	if (_capture && _capture->running) {
		if (_stop_capture(_retired_capture) != OK) {
			return;
		}
		// A live switch preserves the audio server's input ring and read cursor.
		int mix_rate = _input_mix_rate.get();
		Error error = _start_capture(_retired_capture, device.id, mix_rate, true);
		if (error == ERR_ALREADY_IN_USE) {
			// Some legacy drivers only allow one capture handle at a time.
			const WaveDevice previous = _find_device(_input_device, true);
			if (_stop_capture(_capture) != OK) {
				return;
			}
			error = _start_capture(_retired_capture, device.id, mix_rate, true);
			if (error != OK && _start_capture(_capture, previous.id, mix_rate, true) != OK) {
				WARN_PRINT("waveIn could not restore the previous input. Stop and restart capture to retry.");
			}
		}
		if (error != OK) {
			return;
		}
		SWAP(_capture, _retired_capture);
		_stop_capture(_retired_capture);
	}
	_input_device = device.name;
}

void AudioDriverWaveOut::_capture_input() {
	if (!_capture || !_capture->running) {
		return;
	}
	const int channels = _capture->channels;
	for (int i = 0; i < BUFFER_COUNT; ++i) {
		WAVEHDR &header = _capture->headers[_capture->next_buffer];
		if (!(header.dwFlags & WHDR_DONE)) {
			return;
		}
		const int16_t *samples = reinterpret_cast<const int16_t *>(header.lpData);
		const int sample_count = MIN(header.dwBytesRecorded, header.dwBufferLength) / sizeof(int16_t);
		for (int offset = 0; offset + channels <= sample_count; offset += channels) {
			input_buffer_write(int32_t(samples[offset]) * 65536);
			input_buffer_write(int32_t(samples[offset + channels - 1]) * 65536);
		}
		header.dwBytesRecorded = 0;
		const MMRESULT result = waveInAddBuffer(_capture->device, &header, sizeof(header));
		if (result != MMSYSERR_NOERROR) {
			ERR_PRINT_ONCE(vformat("waveIn could not recycle a capture buffer (error %d).", result));
			_stop_capture(_capture);
			return;
		}
		_capture->next_buffer = (_capture->next_buffer + 1) % BUFFER_COUNT;
	}
}

Error AudioDriverWaveOut::_stop_capture(Capture *&r_capture) {
	if (!r_capture) {
		return OK;
	}
	r_capture->running = false;
	if (r_capture->device) {
		waveInReset(r_capture->device);
		for (WAVEHDR &header : r_capture->headers) {
			if (header.dwFlags & WHDR_PREPARED) {
				waveInUnprepareHeader(r_capture->device, &header, sizeof(header));
			}
		}
		const MMRESULT result = waveInClose(r_capture->device);
		if (result != MMSYSERR_NOERROR) {
			// WinMM can hold buffer pointers until the device closes.
			ERR_PRINT_ONCE(vformat("waveIn could not close the input device (error %d).", result));
			return FAILED;
		}
	}
	memdelete(r_capture);
	r_capture = nullptr;
	return OK;
}

Error AudioDriverWaveOut::input_stop() {
	MutexLock lock(_mutex);
	const Error error = _stop_capture(_capture);
	const Error retired_error = _stop_capture(_retired_capture);
	return error == OK ? retired_error : error;
}

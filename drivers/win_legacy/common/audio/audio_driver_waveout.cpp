#include "drivers/win_legacy/common/audio/audio_driver_waveout.h"

#include "core/print_string.h"
#include "core/project_settings.h"

bool AudioDriverWaveOut::_get_pcm_format(int p_mix_rate, int p_channels, WAVEFORMATEX &r_format) {
	const uint32_t block_align = p_channels * sizeof(int16_t);
	if (p_mix_rate <= 0 || uint32_t(p_mix_rate) > UINT32_MAX / block_align) {
		return false;
	}
	r_format = {};
	r_format.wFormatTag = WAVE_FORMAT_PCM;
	r_format.nChannels = p_channels;
	r_format.nSamplesPerSec = p_mix_rate;
	r_format.wBitsPerSample = 16;
	r_format.nBlockAlign = block_align;
	r_format.nAvgBytesPerSec = uint32_t(p_mix_rate) * block_align;
	return true;
}

MMRESULT AudioDriverWaveOut::_open_playback(Playback *&r_playback, UINT p_id, int p_mix_rate) {
	WAVEFORMATEX format = {};
	if (!_get_pcm_format(p_mix_rate, CHANNELS, format)) {
		return WAVERR_BADFORMAT;
	}
	r_playback = memnew(Playback);
	const int samples_per_buffer = _buffer_frames * CHANNELS;
	if (r_playback->samples.resize(samples_per_buffer * BUFFER_COUNT) != OK) {
		return MMSYSERR_NOMEM;
	}
	HWAVEOUT device = nullptr;
	MMRESULT result = waveOutOpen(&device, p_id, &format, reinterpret_cast<DWORD_PTR>(_event), 0, CALLBACK_EVENT);
	if (result != MMSYSERR_NOERROR) {
		return result;
	}
	r_playback->device = device;
	int16_t *samples = r_playback->samples.ptrw();
	for (int i = 0; i < BUFFER_COUNT; ++i) {
		WAVEHDR &header = r_playback->headers[i];
		header.lpData = reinterpret_cast<LPSTR>(samples + i * samples_per_buffer);
		header.dwBufferLength = samples_per_buffer * sizeof(int16_t);
		result = waveOutPrepareHeader(device, &header, sizeof(header));
		if (result != MMSYSERR_NOERROR) {
			return result;
		}
	}
	return MMSYSERR_NOERROR;
}

Error AudioDriverWaveOut::_close_playback(Playback *&r_playback) {
	if (!r_playback) {
		return OK;
	}
	if (r_playback->device) {
		waveOutReset(r_playback->device);
		for (WAVEHDR &header : r_playback->headers) {
			if (header.dwFlags & WHDR_PREPARED) {
				waveOutUnprepareHeader(r_playback->device, &header, sizeof(header));
			}
		}
		const MMRESULT result = waveOutClose(r_playback->device);
		if (result != MMSYSERR_NOERROR) {
			// WinMM can still own the buffers and signal the event until the device closes.
			ERR_PRINT_ONCE(vformat("waveOut could not close the audio device (error %d).", result));
			return FAILED;
		}
	}
	memdelete(r_playback);
	r_playback = nullptr;
	return OK;
}

Error AudioDriverWaveOut::init() {
	ERR_FAIL_COND_V(_event, ERR_ALREADY_IN_USE);
	_stop.clear();
	// A single consumer checks buffer ownership before waiting on this event.
	// The event is unnamed, so the ANSI entry point also serves Windows 9x.
	_event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
	if (!_event) {
		return ERR_CANT_CREATE;
	}

	const int latency_ms = CLAMP(int(GLOBAL_GET("audio/output_latency")), 1, 100);
	_mix_rate = int(GLOBAL_GET("audio/mix_rate"));
	// Keep the total queue near the requested latency while bounding each buffer.
	_buffer_frames = CLAMP(int64_t(_mix_rate) * latency_ms / (1000 * BUFFER_COUNT), 128, 16384);
	MMRESULT result = _open_playback(_playback, WAVE_MAPPER, _mix_rate);
	if (result == WAVERR_BADFORMAT && _mix_rate != DEFAULT_MIX_RATE && _close_playback(_playback) == OK) {
		_mix_rate = DEFAULT_MIX_RATE;
		_buffer_frames = CLAMP(int64_t(_mix_rate) * latency_ms / (1000 * BUFFER_COUNT), 128, 16384);
		result = _open_playback(_playback, WAVE_MAPPER, _mix_rate);
	}
	if (result != MMSYSERR_NOERROR) {
		print_verbose(vformat("waveOut could not open the default audio device (error %d).", result));
		finish();
		return ERR_UNAVAILABLE;
	}
	_input_mix_rate.set(_mix_rate);

	if (_mix_buffer.resize(_buffer_frames * CHANNELS) != OK) {
		finish();
		return ERR_OUT_OF_MEMORY;
	}
	// Godot 3 can tear down and restart the driver when audio is muted.
	// The handle opened above is the mapper, so reapply any named route on the mixer thread.
	if (!_output_device_pending) {
		_requested_output_device = _output_device;
	}
	_output_device = "Default";
	_output_device_pending = _requested_output_device != _output_device;
	return OK;
}

bool AudioDriverWaveOut::_change_output_device() {
	if (!_output_device_pending) {
		return false;
	}
	_output_device_pending = false;
	const WaveDevice device = _find_device(_requested_output_device, false);
	if ((device.name == _output_device && _playback) || _close_playback(_retired_playback) != OK) {
		return false;
	}
	MMRESULT result = _open_playback(_retired_playback, device.id, _mix_rate);
	if (result == MMSYSERR_ALLOCATED && _playback) {
		// Some legacy drivers allow only one playback handle, even for the mapper
		// and its named device. Close first, and restore the old route on failure.
		const WaveDevice previous = _find_device(_output_device, false);
		if (_close_playback(_retired_playback) != OK || _close_playback(_playback) != OK) {
			return false;
		}
		result = _open_playback(_retired_playback, device.id, _mix_rate);
		if (result != MMSYSERR_NOERROR) {
			print_verbose(vformat("waveOut could not switch to %s (error %d).", device.name, result));
			if (_close_playback(_retired_playback) != OK) {
				WARN_PRINT_ONCE("waveOut could not restore playback. Select an output device to retry.");
				return true;
			}
			if (_open_playback(_retired_playback, previous.id, _mix_rate) != MMSYSERR_NOERROR) {
				_close_playback(_retired_playback);
				WARN_PRINT_ONCE("waveOut could not restore playback. Select an output device to retry.");
				return true;
			}
			SWAP(_playback, _retired_playback);
			// The restored route has fresh buffers too; reset the worker's queue index.
			return true;
		}
	}
	if (result != MMSYSERR_NOERROR) {
		print_verbose(vformat("waveOut could not switch to %s (error %d).", device.name, result));
		_close_playback(_retired_playback);
		return false;
	}
	SWAP(_playback, _retired_playback);
	_output_device = device.name;
	_close_playback(_retired_playback);
	return true;
}

void AudioDriverWaveOut::start() {
	if (_playback && !_thread.is_started()) {
		_thread.start(_thread_callback, this);
	}
}

void AudioDriverWaveOut::_thread_callback(void *p_data) {
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
	static_cast<AudioDriverWaveOut *>(p_data)->_play();
}

void AudioDriverWaveOut::_play() {
	const int samples_per_buffer = _buffer_frames * CHANNELS;
	int32_t *mixed = _mix_buffer.ptrw();
	int next_buffer = 0;
	while (!_stop.is_set()) {
		WAVEHDR *header;
		bool output_ready;
		{
			MutexLock lock(_mutex);
			if (_change_output_device()) {
				next_buffer = 0;
			}
			header = _playback ? &_playback->headers[next_buffer] : nullptr;
			output_ready = header && !(header->dwFlags & WHDR_INQUEUE);
			_capture_input();
			if (output_ready) {
				start_counting_ticks();
				audio_server_process(_buffer_frames, mixed);
				stop_counting_ticks();
			}
		}
		if (!output_ready) {
			if (WaitForSingleObject(_event, INFINITE) != WAIT_OBJECT_0) {
				ERR_PRINT("waveOut audio buffer wait failed.");
				return;
			}
			continue;
		}

		int16_t *output = reinterpret_cast<int16_t *>(header->lpData);
		for (int i = 0; i < samples_per_buffer; ++i) {
			output[i] = mixed[i] >> 16;
		}
		const MMRESULT result = waveOutWrite(_playback->device, header, sizeof(*header));
		if (result != MMSYSERR_NOERROR) {
			ERR_PRINT(vformat("waveOut could not submit an audio buffer (error %d).", result));
			return;
		}
		next_buffer = (next_buffer + 1) % BUFFER_COUNT;
	}
}

float AudioDriverWaveOut::get_latency() {
	return float(_buffer_frames * BUFFER_COUNT) / _mix_rate;
}

void AudioDriverWaveOut::finish() {
	if (!_event) {
		return;
	}
	_stop.set();
	if (_thread.is_started()) {
		SetEvent(_event);
		_thread.wait_to_finish();
	}
	MutexLock lock(_mutex);
	const Error capture_error = _stop_capture(_capture);
	const Error retired_capture_error = _stop_capture(_retired_capture);
	const Error playback_error = _close_playback(_playback);
	const Error retired_playback_error = _close_playback(_retired_playback);
	input_buffer.clear();
	_mix_buffer.clear();
	if (capture_error == OK && retired_capture_error == OK && playback_error == OK && retired_playback_error == OK) {
		CloseHandle(_event);
		_event = nullptr;
	}
}

AudioDriverWaveOut::~AudioDriverWaveOut() {
	finish();
}

#pragma once

#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/safe_refcount.h"
#include "servers/audio_server.h"

#include <windows.h>

#include <mmsystem.h>

class AudioDriverWaveOut : public AudioDriver {
	enum {
		BUFFER_COUNT = 3,
		CHANNELS = 2,
		DEFAULT_MIX_RATE = 44100,
	};

	struct Playback {
		WAVEHDR headers[BUFFER_COUNT] = {};
		Vector<int16_t> samples;
		HWAVEOUT device = nullptr;
	};

	struct Capture {
		WAVEHDR headers[BUFFER_COUNT] = {};
		Vector<int16_t> samples;
		HWAVEIN device = nullptr;
		int channels = 0;
		int next_buffer = 0;
		bool running = false;
	};

	struct WaveDevice {
		String name;
		UINT id;
	};

	Thread _thread;
	Mutex _mutex;
	Vector<int32_t> _mix_buffer;
	String _output_device = "Default";
	String _requested_output_device;
	String _input_device = "Default";
	HANDLE _event = nullptr;
	Playback *_playback = nullptr;
	Playback *_retired_playback = nullptr;
	Capture *_capture = nullptr;
	Capture *_retired_capture = nullptr;
	int _mix_rate = DEFAULT_MIX_RATE;
	SafeNumeric<int> _input_mix_rate{ DEFAULT_MIX_RATE };
	int _buffer_frames = 0;
	SafeFlag _stop;
	bool _output_device_pending = false;

	static bool _get_pcm_format(int p_mix_rate, int p_channels, WAVEFORMATEX &r_format);
	static Vector<WaveDevice> _enumerate_devices(bool p_input);
	static Array _get_device_list(bool p_input);
	static WaveDevice _find_device(const String &p_name, bool p_input);
	MMRESULT _open_playback(Playback *&r_playback, UINT p_id, int p_mix_rate);
	static Error _close_playback(Playback *&r_playback);
	bool _change_output_device();
	MMRESULT _open_capture_device(Capture &r_capture, UINT p_id, int &r_mix_rate, bool p_fixed_rate);
	Error _start_capture(Capture *&r_capture, UINT p_id, int &r_mix_rate, bool p_fixed_rate);
	static Error _stop_capture(Capture *&r_capture);
	void _capture_input();
	static void _thread_callback(void *p_data);
	void _play();

public:
	const char *get_name() const override { return "waveOut"; }
	Error init() override;
	void start() override;
	int get_mix_rate() const override { return _mix_rate; }
	int get_input_mix_rate() const override { return _input_mix_rate.get(); }
	SpeakerMode get_speaker_mode() const override { return SPEAKER_MODE_STEREO; }
	float get_latency() override;
	void lock() override { _mutex.lock(); }
	void unlock() override { _mutex.unlock(); }
	Error capture_start() override;
	Error capture_stop() override;
	Array get_device_list() override;
	String get_device() override;
	void set_device(String p_name) override;
	Array capture_get_device_list() override;
	String capture_get_device() override;
	void capture_set_device(const String &p_name) override;
	void finish() override;
	~AudioDriverWaveOut() override;
};

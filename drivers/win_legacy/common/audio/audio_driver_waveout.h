#pragma once

#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/templates/safe_refcount.h"
#include "servers/audio/audio_driver.h"

#include <windows.h>

#include <mmsystem.h>

class AudioDriverWaveOut : public AudioDriver {
	enum {
		BUFFER_COUNT = 3,
		CHANNELS = 2,
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
	int _mix_rate = AudioDriverManager::DEFAULT_MIX_RATE;
	SafeNumeric<int> _input_mix_rate{ AudioDriverManager::DEFAULT_MIX_RATE };
	int _buffer_frames = 0;
	SafeFlag _stop;
	bool _output_device_pending = false;

	static bool _get_pcm_format(int p_mix_rate, int p_channels, WAVEFORMATEX &r_format);
	static Vector<WaveDevice> _enumerate_devices(bool p_input);
	static PackedStringArray _get_device_list(bool p_input);
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
	Error input_start() override;
	Error input_stop() override;
	PackedStringArray get_output_device_list() override;
	String get_output_device() override;
	void set_output_device(const String &p_name) override;
	PackedStringArray get_input_device_list() override;
	String get_input_device() override;
	void set_input_device(const String &p_name) override;
	void finish() override;
	~AudioDriverWaveOut() override;
};

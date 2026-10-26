#include "core/engine.h"
#include "core/input_map.h"
#include "core/message_queue.h"
#include "core/os/time.h"
#include "core/project_settings.h"
#include "core/register_core_types.h"
#include "devices.h"
#include "drivers/fixed_function/tests/test_restricted_root.h"
#include "platform/windows/os_windows.h"
#include "servers/register_server_types.h"

#include <float.h>
#include <stdio.h>

HINSTANCE godot_hinstance = nullptr;

class TestFixedFunctionStandalone {
	class FFOSWindows : public OS_Windows {
	public:
		FFOSWindows() :
				OS_Windows(GetModuleHandleW(nullptr)) {}
		using OS_Windows::finalize_core;
		using OS_Windows::initialize_core;
	};
	static LRESULT CALLBACK _window_proc(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_lparam) {
		return DefWindowProcW(p_window, p_message, p_wparam, p_lparam);
	}
	static bool _save(const char *p_path, const std::vector<uint32_t> &p_pixels) {
		FILE *file = fopen(p_path, "wb");
		if (!file) {
			return false;
		}
		fprintf(file, "P6\n%d %d\n255\n", TestFixedFunctionRestrictedRoot::CAPTURE_WIDTH, TestFixedFunctionRestrictedRoot::CAPTURE_HEIGHT);
		for (size_t i = 0; i < p_pixels.size(); i++) {
			const unsigned char rgb[] = { static_cast<unsigned char>(p_pixels[i] >> 16), static_cast<unsigned char>(p_pixels[i] >> 8), static_cast<unsigned char>(p_pixels[i]) };
			if (fwrite(rgb, 1, 3, file) != 3) {
				fclose(file);
				return false;
			}
		}
		return fclose(file) == 0;
	}

public:
	static int run(int p_argc, char **p_argv) {
		const TestFixedFunctionStandaloneDevices::FFDeviceFactory *factory = p_argc >= 2 ? TestFixedFunctionStandaloneDevices::find_factory(p_argv[1]) : nullptr;
		if (p_argc < 2 || p_argc > 3 || !factory) {
			if (p_argc >= 2 && !factory) {
				fprintf(stderr, "Driver '%s' is not compiled into this test executable.\n", p_argv[1]);
			}
			fprintf(stderr, "Usage: fixed_function_server <");
			const TestFixedFunctionStandaloneDevices::FFDeviceFactory *available = TestFixedFunctionStandaloneDevices::get_factories();
			if (!available) {
				fprintf(stderr, "none");
			}
			for (; available; available = available->get_next()) {
				fprintf(stderr, "%s%s", available->get_name(), available->get_next() ? "|" : "");
			}
			fprintf(stderr, "> [capture.ppm]\n");
			return 2;
		}
		setvbuf(stdout, nullptr, _IONBF, 0);
		godot_hinstance = GetModuleHandleW(nullptr);
		RID_OwnerBase::init_rid();
		// Core-only startup never calls OS_Windows::initialize. Static storage also
		// initializes native fields otherwise initialized by that window-system step.
		static FFOSWindows os;
		os.initialize_core();
		Engine *engine = memnew(Engine);
		register_core_types();
		ProjectSettings *settings = memnew(ProjectSettings);
		InputMap *input_map = memnew(InputMap);
		Time *time = memnew(Time);
		MessageQueue *messages = memnew(MessageQueue);
		register_core_settings();
		settings->set("rendering/gles3/shaders/debug_shader_fallbacks", false);
		settings->set("rendering/threads/thread_safe_bvh", false);
		WNDCLASSW window_class = {};
		window_class.lpfnWndProc = _window_proc;
		window_class.hInstance = GetModuleHandleW(nullptr);
		window_class.lpszClassName = L"FixedFunctionRestrictedRootProbe";
		RegisterClassW(&window_class);
		RECT bounds = { 0, 0, TestFixedFunctionRestrictedRoot::CAPTURE_WIDTH, TestFixedFunctionRestrictedRoot::CAPTURE_HEIGHT };
		AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);
		HWND window = CreateWindowExW(0, window_class.lpszClassName, L"Fixed Function", WS_OVERLAPPEDWINDOW, 0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top, nullptr, nullptr, window_class.hInstance, nullptr);
		FixedFunctionGraphicsDevice *device = factory->create();
		std::vector<uint32_t> pixels;
#if defined(_M_IX86)
		const unsigned int fpu_before = _control87(0, 0) & (_MCW_PC | _MCW_RC);
#endif
		bool success = window && device && TestFixedFunctionRestrictedRoot::test(*device, window, pixels) && (p_argc <= 2 || _save(p_argv[2], pixels));
#if defined(_M_IX86)
		printf("CPU FPU control: before=%x after=%x\n", fpu_before, _control87(0, 0) & (_MCW_PC | _MCW_RC));
		success = success && fpu_before == (_control87(0, 0) & (_MCW_PC | _MCW_RC));
#endif
		if (device) {
			memdelete(device);
		}
		if (window) {
			DestroyWindow(window);
		}
		UnregisterClassW(window_class.lpszClassName, window_class.hInstance);
		memdelete(messages);
		memdelete(time);
		memdelete(input_map);
		memdelete(settings);
		unregister_core_types();
		memdelete(engine);
		os.finalize_core();
		return success ? 0 : 1;
	}
};

int main(int p_argc, char **p_argv) {
	return TestFixedFunctionStandalone::run(p_argc, p_argv);
}

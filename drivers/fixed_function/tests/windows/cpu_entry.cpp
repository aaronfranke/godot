#include "core/engine.h"
#include "core/os/time.h"
#include "core/project_settings.h"
#include "core/register_core_types.h"
#include "drivers/fixed_function/tests/test_scene_cpu.h"
#include "platform/windows/os_windows.h"

#include <stdio.h>
#include <string.h>

HINSTANCE godot_hinstance = nullptr;

class TestFixedFunctionCPUStandalone {
	class FFOSWindows : public OS_Windows {
	public:
		FFOSWindows() :
				OS_Windows(GetModuleHandleW(nullptr)) {}
		using OS_Windows::finalize_core;
		using OS_Windows::initialize_core;
	};

public:
	static int run(int p_argc, char **p_argv) {
		if (p_argc > 2 || (p_argc == 2 && strcmp(p_argv[1], "--benchmark"))) {
			fprintf(stderr, "Usage: fixed_function_cpu [--benchmark]\n");
			return 2;
		}
		godot_hinstance = GetModuleHandleW(nullptr);
		RID_OwnerBase::init_rid();
		// Core-only OS startup: no native window or graphics device is initialized.
		static FFOSWindows os;
		os.initialize_core();
		Engine *engine = memnew(Engine);
		register_core_types();
		ProjectSettings *settings = memnew(ProjectSettings);
		Time *time = memnew(Time);
		register_core_settings();
		const bool success = TestFixedFunctionSceneCPU::test(p_argc == 2);
		memdelete(time);
		memdelete(settings);
		unregister_core_types();
		memdelete(engine);
		os.finalize_core();
		return success ? 0 : 1;
	}
};

int main(int p_argc, char **p_argv) {
	return TestFixedFunctionCPUStandalone::run(p_argc, p_argv);
}

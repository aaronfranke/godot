#include "core/os/os.h"
#include "drivers/fixed_function/renderer_utilities_fixed_function.h"
#include "drivers/fixed_function/tests/test_fixed_function_scene_cpu.h"

#include <stdio.h>

#define FF_TIMESTAMP_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Timestamp FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionSceneCPU::test_timestamps() {
	FFUnusedDevice unused;
	FixedFunctionRenderContext context(unused, nullptr, 128, 96, false);
	FixedFunctionRenderTargetManager targets(context);
	RendererTextureStorageFixedFunction textures(context.get_texture_storage(), &targets);
	RendererMaterialStorageFixedFunction materials(context);
	RendererLightStorageFixedFunction lights(context);
	RendererSceneRenderFixedFunction scene(context, targets);
	RendererUtilitiesFixedFunction utilities(context, textures, materials, lights, scene);
	FF_TIMESTAMP_CHECK(utilities.get_captured_timestamps_count() == 0);
	utilities.begin_frame_timestamps();
	FF_TIMESTAMP_CHECK(utilities.get_captured_timestamps_count() == 1);
	// Viewport measurements work independently of the full-frame profiler toggle.
	utilities.capture_timestamp("vp_begin_1");
	const uint64_t begin = utilities.get_captured_timestamp_cpu_time(1);
	OS::get_singleton()->delay_usec(2000);
	utilities.capture_timestamp("vp_end_1");
	utilities.end_frame_timestamps();
	FF_TIMESTAMP_CHECK(utilities.get_captured_timestamps_count() == 4);
	const uint64_t elapsed = utilities.get_captured_timestamp_cpu_time(2) - begin;
	FF_TIMESTAMP_CHECK(elapsed >= 1000 && utilities.get_captured_timestamp_gpu_time(0) == 0);
	FF_TIMESTAMP_CHECK(utilities.get_captured_timestamp_gpu_time(2) - utilities.get_captured_timestamp_gpu_time(1) == elapsed * 1000);
	FF_TIMESTAMP_CHECK(utilities.get_captured_timestamp_name(1) == "vp_begin_1" && utilities.get_captured_timestamp_name(2) == "vp_end_1");
	// CPU samples need no GPU-query limit. Growing storage must preserve earlier rows.
	for (int i = 0; i < 300; i++) {
		utilities.capture_timestamp("marker");
	}
	FF_TIMESTAMP_CHECK(utilities.get_captured_timestamps_count() == 304 && utilities.get_captured_timestamp_cpu_time(1) == begin);
	utilities.begin_frame_timestamps();
	utilities.end_frame_timestamps();
	FF_TIMESTAMP_CHECK(utilities.get_captured_timestamps_count() == 2);
	utilities.capturing_timestamps = true;
	utilities.begin_frame_timestamps();
	utilities.capture_timestamps_begin();
	utilities.end_frame_timestamps();
	FF_TIMESTAMP_CHECK(utilities.get_captured_timestamps_count() == 3);
	FF_TIMESTAMP_CHECK(utilities.get_captured_timestamp_name(0) == "Internal Begin" && utilities.get_captured_timestamp_name(1) == "Frame Begin" && utilities.get_captured_timestamp_name(2) == "Internal End");
	FF_TIMESTAMP_CHECK(utilities.get_captured_timestamps_frame() == context.get_frame_number());
	utilities.begin_frame_timestamps();
	FF_TIMESTAMP_CHECK(utilities.get_captured_timestamps_count() == 1 && utilities.get_captured_timestamp_name(0) == "Internal Begin");
	utilities.capturing_timestamps = false;
	utilities.begin_frame_timestamps();
	utilities.end_frame_timestamps();
	FF_TIMESTAMP_CHECK(utilities.get_captured_timestamps_count() == 2);
	printf("PASS: CPU timestamp units/order, GPU-slot estimate, viewport-only capture, profiler markers, storage growth and frame reset without native device calls.\n");
	return true;
}

#undef FF_TIMESTAMP_CHECK

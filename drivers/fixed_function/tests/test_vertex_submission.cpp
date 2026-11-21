#include "test_vertex_submission.h"

#include "core/os/os.h"

#include <stdio.h>

#include <algorithm>
#include <vector>

bool TestFixedFunctionVertexSubmission::benchmark(FixedFunctionGraphicsDevice &p_device, const char *p_path) {
	if (!OS::get_singleton()->get_cmdline_args().find("--vertex-stream-benchmark")) {
		return true;
	}
	// A small target and an explicit completion read after every sample bound queued work.
	// These are native submission/completion wall times, not GPU timestamps or scene FPS.
	if (p_device.resize(64, 64) != OK || p_device.prepare_resources() != OK) {
		return false;
	}
	std::vector<FixedFunctionVertex> vertices(256 * 3);
	std::vector<FixedFunctionLightingVertex> lit_vertices(vertices.size());
	for (uint32_t triangle = 0; triangle < 256; triangle++) {
		const float x = float(triangle % 16) / 8.0f - 1.0f;
		const float y = float(triangle / 16) / 8.0f - 1.0f;
		for (uint32_t corner = 0; corner < 3; corner++) {
			const uint32_t index = triangle * 3 + corner;
			FixedFunctionVertex &vertex = vertices[index];
			vertex.x = x + (corner == 2 ? 0.1f : 0);
			vertex.y = y + (corner == 1 ? 0.1f : 0);
			vertex.color = 0xffffffff;
			FixedFunctionLightingVertex &lit = lit_vertices[index];
			lit.x = vertex.x;
			lit.y = vertex.y;
			lit.nz = lit.r = lit.g = lit.b = lit.a = 1;
		}
	}
	FixedFunctionGraphicsDevice::FFPixelLighting lighting;
	lighting.ambient = Color(1, 1, 1);
	lighting.light_count = 0;
	const Projection projection;
	const FixedFunctionGraphicsDevice::FFDrawParameters state;
	std::vector<uint32_t> pixels(64 * 64);
	const int profiles = p_device.get_max_per_pixel_lights() ? 2 : 1;
	for (int profile = 0; profile < profiles; profile++) {
		std::vector<uint64_t> submission;
		std::vector<uint64_t> completion;
		for (int sample = 0; sample < 10; sample++) {
			const uint64_t start = OS::get_singleton()->get_ticks_usec();
			if (p_device.begin_frame(0xff000000) != OK) {
				return false;
			}
			for (int draw = 0; draw < 16; draw++) {
				const Error error = profile ? p_device.draw_lit_triangles(lit_vertices.data(), 768, projection, lighting, RID(), state) : p_device.draw_triangles(vertices.data(), 768, projection);
				if (error != OK) {
					return false;
				}
			}
			if (p_device.end_frame(false) != OK) {
				return false;
			}
			const uint64_t submitted = OS::get_singleton()->get_ticks_usec() - start;
			if (p_device.read_frame(pixels.data(), pixels.size()) != OK) {
				return false;
			}
			const uint64_t completed = OS::get_singleton()->get_ticks_usec() - start;
			if (sample) {
				submission.push_back(submitted);
				completion.push_back(completed);
			}
		}
		std::sort(submission.begin(), submission.end());
		std::sort(completion.begin(), completion.end());
		printf("PERF3D_NATIVE path=%s stride=%u triangles=4096 draws=16 samples=9 submit_median_us=%llu complete_median_us=%llu complete_max_us=%llu\n", p_path, profile ? 48u : 24u, static_cast<unsigned long long>(submission[4]), static_cast<unsigned long long>(completion[4]), static_cast<unsigned long long>(completion[8]));
	}
	return true;
}

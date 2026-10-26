#include "test_resolution_policy_d3d7.h"

#include <stdio.h>
#include <string.h>

#define CHECK(m_condition)                                                           \
	do {                                                                             \
		if (!(m_condition)) {                                                        \
			printf("D3D7 resolution FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                            \
		}                                                                            \
	} while (false)

void TestD3D7ResolutionPolicy::_branch(std::vector<uint8_t> &r_image, unsigned p_offset, unsigned p_destination) {
	const int32_t displacement = int32_t(p_destination) - int32_t(p_offset) - 6;
	memcpy(r_image.data() + p_offset + 2, &displacement, 4);
}

std::vector<uint8_t> TestD3D7ResolutionPolicy::_fixture(unsigned p_shift) {
	// Independent instruction excerpts from the inspected runtime, relocated into a
	// synthetic PE. The recognizer must follow branches, not depend on original RVAs.
	static const uint8_t CONTEXT0[] = { 0xc7, 0x45, 0x84, 0x6c, 0x00, 0x00, 0x00, 0x8b, 0x70, 0x58, 0x8d, 0x45, 0x84, 0x50, 0xff, 0xb5, 0x70, 0xff, 0xff, 0xff, 0x8b, 0xce, 0xff, 0x15, 0x4c, 0x21, 0x06, 0x10, 0xff, 0xd6, 0x85, 0xc0, 0x0f, 0x85, 0x57, 0x04, 0x00, 0x00, 0xf7, 0x45, 0xec, 0x00, 0x20, 0x00, 0x00, 0x0f, 0x84, 0x45, 0x04, 0x00, 0x00, 0xf7, 0x45, 0xec, 0x00, 0x00, 0x02, 0x00, 0x0f, 0x85, 0x38, 0x04, 0x00, 0x00 };
	static const uint8_t CONTEXT1[] = { 0xc7, 0x85, 0x38, 0xff, 0xff, 0xff, 0x7c, 0x00, 0x00, 0x00, 0x8b, 0x70, 0x58, 0x8d, 0x85, 0x38, 0xff, 0xff, 0xff, 0x50, 0x53, 0x8b, 0xce, 0xff, 0x15, 0x4c, 0x21, 0x06, 0x10, 0xff, 0xd6, 0x8b, 0xf0, 0x85, 0xf6, 0x0f, 0x85, 0x1c, 0x03, 0x00, 0x00, 0xf7, 0x45, 0xa0, 0x00, 0x20, 0x00, 0x00, 0x0f, 0x84, 0x96, 0x02, 0x00, 0x00, 0x8b, 0x4d, 0x8c, 0xe8, 0xbe, 0xa3, 0xff, 0xff, 0x8b, 0x8f, 0xd0, 0x22, 0x00, 0x00, 0x85, 0x81, 0xa0, 0x00, 0x00, 0x00, 0x0f, 0x84, 0x7c, 0x02, 0x00, 0x00 };
	static const uint8_t CHECK0[] = { 0xb8, 0x00, 0x08, 0x00, 0x00, 0x39, 0x45, 0x90, 0x0f, 0x87, 0x23, 0x04, 0x00, 0x00, 0x39, 0x45, 0x8c, 0x0f, 0x87, 0x1a, 0x04, 0x00, 0x00 };
	static const uint8_t CHECK1[] = { 0xb8, 0x00, 0x08, 0x00, 0x00, 0x39, 0x85, 0x44, 0xff, 0xff, 0xff, 0x0f, 0x87, 0x64, 0x02, 0x00, 0x00, 0x39, 0x85, 0x40, 0xff, 0xff, 0xff, 0x0f, 0x87, 0x58, 0x02, 0x00, 0x00 };
	static const uint8_t ERROR0[] = { 0xb8, 0x82, 0x00, 0x76, 0x88, 0xeb, 0x05, 0xb8, 0x64, 0x00, 0x76, 0x88 };
	static const uint8_t ERROR1[] = { 0xbe, 0x57, 0x00, 0x07, 0x80, 0xeb, 0x79, 0xbe, 0x64, 0x00, 0x76, 0x88 };
	std::vector<uint8_t> image(0x4000, 0);
	IMAGE_DOS_HEADER *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(image.data());
	dos->e_magic = IMAGE_DOS_SIGNATURE;
	dos->e_lfanew = 0x80;
	IMAGE_NT_HEADERS32 *nt = reinterpret_cast<IMAGE_NT_HEADERS32 *>(image.data() + 0x80);
	nt->Signature = IMAGE_NT_SIGNATURE;
	nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
	nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER32);
	nt->FileHeader.NumberOfSections = 1;
	nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
	nt->OptionalHeader.SizeOfImage = image.size();
	IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt);
	section->VirtualAddress = 0x1000;
	section->Misc.VirtualSize = 0x3000;
	section->Characteristics = IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;
	const unsigned context0 = 0x1400 + p_shift, context1 = 0x2200 + p_shift;
	const unsigned site0 = context0 + sizeof(CONTEXT0), site1 = context1 + sizeof(CONTEXT1);
	const unsigned error0 = 0x1800 + p_shift, error1 = 0x2600 + p_shift;
	memcpy(image.data() + context0, CONTEXT0, sizeof(CONTEXT0));
	memcpy(image.data() + context1, CONTEXT1, sizeof(CONTEXT1));
	memcpy(image.data() + site0, CHECK0, sizeof(CHECK0));
	memcpy(image.data() + site1, CHECK1, sizeof(CHECK1));
	memcpy(image.data() + error0, ERROR0, sizeof(ERROR0));
	memcpy(image.data() + error1, ERROR1, sizeof(ERROR1));
	_branch(image, site0 + 8, error0);
	_branch(image, site0 + 17, error0);
	_branch(image, site0 - 19, error0 + 7);
	_branch(image, site0 - 6, error0 + 7);
	_branch(image, site1 + 11, error1);
	_branch(image, site1 + 23, error1);
	_branch(image, site1 - 32, error1 + 7);
	_branch(image, site1 - 6, error1 + 7);
	return image;
}

bool TestD3D7ResolutionPolicy::test() {
	D3D7ResolutionPolicy restricted;
	CHECK(restricted.get_limit() == 2048 && restricted.get_max_window_size() == Size2i(2048, 2048));
	CHECK(restricted.get_render_size(3840, 2160) == Size2i(2048, 1152));
	CHECK(restricted.get_render_size(2160, 3840) == Size2i(1152, 2048));
	CHECK(restricted.get_render_size(1920, 1080) == Size2i(1920, 1080));
	CHECK(restricted.get_render_size(99999, 1) == Size2i(2048, 1));
	CHECK(restricted.get_render_size(4096, 2048, 1024, 256) == Size2i(512, 256));
	CHECK(restricted.get_render_size(0, 100) == Size2i());
	CHECK(restricted.get_render_size(UINT32_MAX, UINT32_MAX) == Size2i());
	D3D7ResolutionPolicy stretch(D3D7ResolutionPolicy::D3D7_OVERSIZE_STRETCH);
	CHECK(stretch.get_max_window_size() == Size2i());
	D3D7ResolutionPolicy automatic(D3D7ResolutionPolicy::D3D7_OVERSIZE_OVERRIDE);
	CHECK(automatic.configure(16384, 16384, Size2i(1920, 1080)) == OK && automatic.get_limit() == 2048);
	D3D7ResolutionPolicy force(D3D7ResolutionPolicy::D3D7_OVERSIZE_FORCE_OVERRIDE);
	CHECK(force.configure(1024, 1024, Size2i(3840, 2160)) == OK && force.get_limit() == 2048);
	CHECK(D3D7ResolutionPolicy::get_native_clip(Rect2i(10, 20, 30, 40), Size2i(4096, 2048), Size2i(2048, 1024)) == Rect2i(5, 10, 15, 20));
	uint32_t sites[2];
	std::vector<uint8_t> image = _fixture();
	CHECK(D3D7ResolutionPolicy::_find_sites(image.data(), image.size(), sites));
	CHECK(sites[0] == 0x1441 && sites[1] == 0x2251);
	image = _fixture(0x100);
	image[0x1300] = 0x42; // Unrelated code/data changes must not invalidate the profile.
	CHECK(D3D7ResolutionPolicy::_find_sites(image.data(), image.size(), sites));
	CHECK(sites[0] == 0x1541 && sites[1] == 0x2351);
	const unsigned mutations[] = { 0x1442, 0x1449, 0x1453, 0x1446, 0x1403, 0x1800, 0x225c, 0x2600 };
	for (unsigned i = 0; i < sizeof(mutations) / sizeof(mutations[0]); i++) {
		image = _fixture();
		image[mutations[i]] ^= 1;
		CHECK(!D3D7ResolutionPolicy::_find_sites(image.data(), image.size(), sites));
	}
	image = _fixture();
	memcpy(image.data() + 0x3000, image.data() + 0x1400, 64 + 23);
	memcpy(image.data() + 0x3400, image.data() + 0x1800, 12);
	CHECK(!D3D7ResolutionPolicy::_find_sites(image.data(), image.size(), sites));
	image = _fixture();
	IMAGE_NT_HEADERS32 *nt = reinterpret_cast<IMAGE_NT_HEADERS32 *>(image.data() + 0x80);
	IMAGE_FIRST_SECTION(nt)->Characteristics = IMAGE_SCN_MEM_READ;
	CHECK(!D3D7ResolutionPolicy::_find_sites(image.data(), image.size(), sites));
	IMAGE_FIRST_SECTION(nt)->Characteristics |= IMAGE_SCN_MEM_EXECUTE;
	IMAGE_FIRST_SECTION(nt)->Misc.VirtualSize = UINT32_MAX;
	CHECK(!D3D7ResolutionPolicy::_find_sites(image.data(), image.size(), sites));
	CHECK(!D3D7ResolutionPolicy::_find_sites(image.data(), 1, sites));
	printf("PASS: D3D7 bounded sizing, no-override 1080p, relocated instruction profiles and malformed/ambiguous rejection.\n");
	return true;
}

bool TestD3D7ResolutionPolicy::test_device(FixedFunctionGraphicsDeviceD3D7 &p_device, HWND p_window) {
	CHECK(p_device.initialize(p_window, 4096, 256, false) == OK);
	CHECK(p_device.get_width() == 4096 && p_device.get_height() == 256);
	CHECK(p_device.get_native_window_size() == Size2i(2048, 128));
	RID target;
	CHECK(p_device.texture_create_render_target(4096, 512, target) == OK);
	FixedFunctionGraphicsDevice::FFTextureInfo info;
	CHECK(p_device.texture_get_info(target, info) == OK && info.size == Size2i(4096, 512));
	CHECK(info.bytes > 0 && info.bytes <= uint64_t(2048) * 256 * 4);
	CHECK(p_device.begin_frame(0xff0000ff, target) == OK);
	const FixedFunctionVertex quad[] = {
		{ -1, 1, 0, 0xffff0000, 0, 0 }, { 1, 1, 0, 0xffff0000, 1, 0 }, { 1, -1, 0, 0xffff0000, 1, 1 }, { -1, 1, 0, 0xffff0000, 0, 0 }, { 1, -1, 0, 0xffff0000, 1, 1 }, { -1, -1, 0, 0xffff0000, 0, 1 }
	};
	CameraMatrix projection;
	FixedFunctionGraphicsDevice::FFDrawParameters parameters;
	parameters.clip_enabled = true;
	parameters.clip_rect = Rect2i(0, 0, 2048, 512);
	CHECK(p_device.draw_triangles(quad, 6, projection, RID(), parameters) == OK);
	CHECK(p_device.end_frame(false) == OK);
	CHECK(p_device.begin_frame(0xff000000) == OK);
	FixedFunctionVertex sampled_quad[6];
	memcpy(sampled_quad, quad, sizeof(quad));
	for (unsigned i = 0; i < 6; i++) {
		sampled_quad[i].color = 0xffffffff;
	}
	CHECK(p_device.draw_triangles(sampled_quad, 6, projection, target) == OK);
	CHECK(p_device.end_frame(false) == OK);
	std::vector<uint32_t> pixels(4096 * 256);
	CHECK(p_device.read_frame(pixels.data(), pixels.size()) == OK);
	const uint32_t left = pixels[128 * 4096 + 1024];
	const uint32_t right = pixels[128 * 4096 + 3072];
	// Sampling the viewport can round the modulated channel down by one on RGB.
	CHECK((left == 0xffff0000 || left == 0xfffe0000) && (right == 0xff0000ff || right == 0xff0000fe));
	CHECK(p_device.texture_free(target) == OK);
	if (p_device.get_capabilities().max_texture_width >= 3000 && !p_device.get_capabilities().texture_power_of_two && !p_device.get_capabilities().texture_square_only) {
		Ref<Image> image;
		image.instance();
		image->create(3000, 1, false, Image::FORMAT_RGBA8);
		RID ordinary;
		CHECK(p_device.texture_create(image, ordinary) == OK);
		CHECK(p_device.texture_get_info(ordinary, info) == OK && info.size == Size2i(3000, 1));
		CHECK(p_device.texture_free(ordinary) == OK);
	}
	CHECK(p_device.finalize() == OK);
	printf("PASS: D3D7 oversized logical window/viewport, native allocation accounting, clipped drawing and sampled stretched output.\n");
	return true;
}

bool TestD3D7ResolutionPolicy::test_mode(HWND p_window, D3D7ResolutionPolicy::D3D7OversizeMode p_mode, bool p_block_override, bool p_force_rgb) {
	FixedFunctionGraphicsDeviceD3D7 device(p_mode, p_force_rgb);
	if (p_block_override) {
		InterlockedExchange(&D3D7ResolutionPolicy::_patch_lock, 1);
	}
	const Error initialized = device.initialize(p_window, 4096, 256, false);
	if (p_block_override) {
		InterlockedExchange(&D3D7ResolutionPolicy::_patch_lock, 0);
	}
	CHECK(initialized == OK);
	const uint32_t limit = device.get_render_target_limit();
	CHECK(!p_block_override || limit == 2048);
	CHECK(limit >= 2048 && (limit == 2048 || limit <= MIN(device.get_capabilities().max_texture_width, device.get_capabilities().max_texture_height)));
	CHECK(device.get_native_window_size() == Size2i(MIN(limit, uint32_t(4096)), MIN(limit, uint32_t(4096)) / 16));
	CHECK((device.get_max_window_size() != Size2i()) == (p_mode == D3D7ResolutionPolicy::D3D7_OVERSIZE_RESTRICTED));
	RID target;
	CHECK(device.texture_create_render_target(4096, 512, target) == OK);
	CHECK(device.begin_frame(0xff123456, target) == OK);
	CHECK(device.end_frame(false) == OK);
	CHECK(device.texture_is_render_target_ready(target));
	CHECK(device.texture_free(target) == OK);
	CHECK(device.begin_frame(0xff123456) == OK);
	CHECK(device.end_frame(false) == OK);
	std::vector<uint32_t> pixels(4096 * 256);
	CHECK(device.read_frame(pixels.data(), pixels.size()) == OK);
	CHECK(pixels[128 * 4096 + 2000] == 0xff123456);
	printf("PASS: D3D7 mode=%u limit=%u native=%dx%d logical=4096x256\n", unsigned(p_mode), limit, device.get_native_window_size().x, device.get_native_window_size().y);
	CHECK(device.finalize() == OK);
	return true;
}

#undef CHECK

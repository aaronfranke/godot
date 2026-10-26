#include "graphics_device_opengl1_factory.h"

#include "core/os/os.h"
#include "drivers/opengl1/graphics_device_opengl1.h"
#include "drivers/opengl1/wgl_context.h"

class FixedFunctionGraphicsDeviceOpenGL1Windows : public FixedFunctionGraphicsDeviceOpenGL1 {
	static uint8_t _get_optional_features() {
		uint8_t features = FEATURE_ALL;
		const List<String> arguments = OS::get_singleton()->get_cmdline_args();
		for (const List<String>::Element *argument = arguments.front(); argument; argument = argument->next()) {
			if (argument->get() == "--opengl1-disable-npot") {
				features &= ~FEATURE_NPOT_TEXTURE;
			}
		}
		return features;
	}

public:
	FixedFunctionGraphicsDeviceOpenGL1Windows() :
			FixedFunctionGraphicsDeviceOpenGL1(memnew(FixedFunctionWGLContext), _get_optional_features()) {}

	Error initialize(void *p_window, uint32_t p_width, uint32_t p_height, bool p_vsync) override {
		const Error error = FixedFunctionGraphicsDeviceOpenGL1::initialize(p_window, p_width, p_height, p_vsync);
		if (error != OK) {
			return error;
		}
		const FFCapabilities &caps = get_capabilities();
		if (!caps.render_target_opaque) {
			ERR_PRINT("OpenGL1 requires framebuffer objects for Godot viewports.");
			const Error finalized = FixedFunctionGraphicsDeviceOpenGL1::finalize();
			return finalized == OK ? ERR_UNAVAILABLE : finalized;
		}
		return OK;
	}
};

FixedFunctionGraphicsDevice *FixedFunctionGraphicsDeviceOpenGL1Factory::create() {
	return memnew(FixedFunctionGraphicsDeviceOpenGL1Windows);
}

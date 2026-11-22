#ifndef TEST_FIXED_FUNCTION_LIGHT_STORAGE_H
#define TEST_FIXED_FUNCTION_LIGHT_STORAGE_H

#include "drivers/fixed_function/render_context.h"

class TestFixedFunctionLightStorage {
	struct FFInstance : RasterizerScene::InstanceBase {
		FixedFunctionLightStorage &storage;
		RID renderer_instance;
		FFInstance *other;
		int changes;
		int removals;
		bool mutate;
		bool rebind_other;
		bool clear_on_change;
		bool success;
		FFInstance(FixedFunctionLightStorage &p_storage, RID p_light);
		~FFInstance() override;
		void base_changed(bool p_aabb, bool p_materials) override;
		void base_removed() override;
	};
	static bool _test_metadata(FixedFunctionLightStorage &p_storage);
	static bool _test_dependencies(FixedFunctionLightStorage &p_storage);
	static bool _test_facades(FixedFunctionRenderContext &p_context);

public:
	static bool test(FixedFunctionRenderContext &p_context);
};

#endif // TEST_FIXED_FUNCTION_LIGHT_STORAGE_H

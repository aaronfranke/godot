#ifndef TEST_FIXED_FUNCTION_IMMEDIATE_GEOMETRY_H
#define TEST_FIXED_FUNCTION_IMMEDIATE_GEOMETRY_H

#include "drivers/fixed_function/render_context.h"

class TestFixedFunctionImmediateGeometry {
	class FFInstance : public RasterizerScene::InstanceBase {
	public:
		enum FFAction {
			ACTION_NONE,
			ACTION_DIRTY,
			ACTION_FREE,
			ACTION_CLEAR,
			ACTION_REBIND
		};
		FixedFunctionMeshStorage &storage;
		FFInstance *other;
		RID replacement;
		int changed;
		int removed;
		FFAction action;
		bool success;
		bool aabb_changed;
		bool materials_changed;
		void base_changed(bool p_aabb, bool p_materials) override;
		void base_removed() override;
		FFInstance(FixedFunctionMeshStorage &p_storage, RID p_base);
		~FFInstance() override;
	};
	static bool _data(FixedFunctionMeshStorage &p_meshes);
	static bool _dependencies(FixedFunctionMeshStorage &p_meshes, FixedFunctionMaterialStorage &p_materials);
	static bool _triangle(FixedFunctionMeshStorage &p_meshes, RID p_immediate, const Vector3 &p_origin = Vector3());

public:
	static bool cpu();
};

#endif // TEST_FIXED_FUNCTION_IMMEDIATE_GEOMETRY_H

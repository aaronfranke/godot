#ifndef TEST_FIXED_FUNCTION_MESH_STORAGE_H
#define TEST_FIXED_FUNCTION_MESH_STORAGE_H

#include "drivers/fixed_function/mesh_storage.h"

class TestFixedFunctionMeshStorage {
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
		bool aabb;
		bool materials;
		bool success;
		void base_changed(bool p_aabb, bool p_materials) override;
		void base_removed() override;
		FFInstance(FixedFunctionMeshStorage &p_storage, RID p_mesh);
		~FFInstance() override;
	};
	static PoolVector<uint8_t> _vertices(int p_count = 3);
	static Error _add(FixedFunctionMeshStorage &p_storage, RID p_mesh, const AABB &p_aabb = AABB(Vector3(-1, -1, 0), Vector3(2, 2, 0)));
	static bool _test_data(FixedFunctionMeshStorage &p_storage);
	static bool _test_formats(FixedFunctionMeshStorage &p_storage);
	static bool _test_dependencies(FixedFunctionMeshStorage &p_storage, FixedFunctionMaterialStorage &p_materials);

public:
	static bool test();
};

#endif // TEST_FIXED_FUNCTION_MESH_STORAGE_H

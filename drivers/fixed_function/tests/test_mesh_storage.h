#pragma once

#include "drivers/fixed_function/mesh_storage.h"

class TestFixedFunctionMeshStorage {
	struct FFDependencyState {
		FixedFunctionMeshStorage *storage;
		RID mesh;
		RID other;
		int changed;
		int deleted;
		int action;
		bool success;
		FFDependencyState();
	};
	static void _changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker);
	static void _deleted(const RID &p_rid, DependencyTracker *p_tracker);
	static bool _test_layouts(FixedFunctionMeshStorage &p_meshes);
	static bool _test_dependencies(FixedFunctionMeshStorage &p_meshes);
	static bool _test_large_indices(FixedFunctionMeshStorage &p_meshes);
	static bool _test_revisions(FixedFunctionMeshStorage &p_meshes);

public:
	static bool test();
};

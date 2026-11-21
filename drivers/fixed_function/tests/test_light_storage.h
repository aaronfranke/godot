#pragma once

#include "drivers/fixed_function/render_context.h"

class TestFixedFunctionLightStorage {
	struct FFObserver {
		DependencyTracker tracker;
		FixedFunctionLightStorage *storage;
		RID light;
		int changes;
		int deletions;
		Error error;
		bool mutate;
		bool release;
		bool clear;
		FFObserver();
	};
	static void _changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker);
	static void _deleted(const RID &p_dependency, DependencyTracker *p_tracker);
	static bool _dependencies(FixedFunctionLightStorage &p_storage);

public:
	static bool test(FixedFunctionRenderContext &p_context);
};

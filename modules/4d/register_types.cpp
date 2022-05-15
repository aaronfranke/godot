#include "register_types.h"

#include "nodes/node_4d.h"

void initialize_4d_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_class<Node4D>();
}

void uninitialize_4d_module(ModuleInitializationLevel p_level) {}

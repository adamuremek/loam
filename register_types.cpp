#include "register_types.h"
#include "core/object/class_db.h";
#include "fart_node.h";

void initialize_gdnet_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	ClassDB::register_class<FartNode>();
}

void uninitialize_gdnet_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

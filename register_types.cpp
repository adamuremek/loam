#include "register_types.h"
#include "core/object/class_db.h"
#include "gdnet.h"

 static GDNet *p_gdnetSingleton = nullptr;

void initialize_gdnet_module(ModuleInitializationLevel p_level) {
	 if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
	 	return;
	 }

	 ClassDB::register_class<World>();
	 ClassDB::register_class<Zone>();
	 ClassDB::register_class<Player>();
	 ClassDB::register_class<GDNet>();

	 p_gdnetSingleton = memnew(GDNet);
	 Engine::get_singleton()->add_singleton(Engine::Singleton("GDNet", GDNet::get_singleton().ptr(), "GDNet"));
}

void uninitialize_gdnet_module(ModuleInitializationLevel p_level) {
	 if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
	 	return;
	 }

	 memdelete(p_gdnetSingleton);
}

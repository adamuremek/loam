#include "register_types.h"
#include "core/object/class_db.h"
#include "gdnet.h"

 static GDNet *p_gdnetSingleton = nullptr;

void initialize_gdnet_module(ModuleInitializationLevel p_level) {
	 if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
	 	return;
	 }

	 ClassDB::register_class<GDNet>();
	 ClassDB::register_class<PlayerInfo>();
	 ClassDB::register_class<EntityInfo>();
	 ClassDB::register_class<NetworkEntity>();
	 ClassDB::register_class<Zone>();
	 ClassDB::register_class<World>();

	 p_gdnetSingleton = memnew(GDNet);
	 GDNet::singleton = p_gdnetSingleton;

	 Engine::get_singleton()->add_singleton(Engine::Singleton("GDNet", GDNet::singleton, "GDNet"));
}

void uninitialize_gdnet_module(ModuleInitializationLevel p_level) {
	 if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
	 	return;
	 }

	 GDNet::singleton->cleanup();
	 GDNet::singleton = nullptr;
	 memdelete(p_gdnetSingleton);
}

#include "gdnet.h"
#include "core/io/dir_access.h"
#include "include/steam/steamnetworkingsockets.h"

GDNet* GDNet::singleton = nullptr;
ZoneID_t GDNet::m_zoneIDCounter = 1;

GDNet::GDNet() {
	world = memnew(World);
	m_isInitialized = false;
	m_isClient = false;
	m_isServer = false;
}

GDNet::~GDNet() {
	memdelete(world);
}

void GDNet::cleanup() {
	world->cleanup();
}

void GDNet::_bind_methods() {
	ClassDB::bind_method("init_gdnet", &GDNet::init_gdnet);
	ClassDB::bind_method("shutdown_gdnet", &GDNet::shutdown_gdnet);
	ClassDB::bind_method("get_world_singleton", &GDNet::get_world_singleton);
}

bool GDNet::register_network_entities() {
	String path = "res://NetworkEntities/";

	//Try opening the "NetworkEntities" special directory.
	Error openErr = OK;
	Ref<DirAccess> dir = DirAccess::open(path, &openErr);

	if (openErr != OK) {
		ERR_PRINT("Failed to find the 'NetworkEntities' directory!");
		return false;
	}

	//Define an id counter for assigning entity ids
	EntityID_t newEntityId = 1;

	//Start directory listing
	dir->list_dir_begin();

	//Iterate through files in the directory
	String file_name = dir->_get_next();
	while (file_name != "") {
		//If the current file is a directory,
		if (dir->current_is_dir()) {
			file_name = dir->_get_next();
			continue;
		}

		//When a scene is found, open it and evaluate it
		if (file_name.get_extension() == "tscn" || file_name.get_extension() == "scn") {
			//Get the packedscene from the scene's full path
			String full_path = path.path_join(file_name);
			Ref<PackedScene> network_entity_scene = ResourceLoader::load(full_path);

			// Verify that the packed scene is a network entity type, then add it to the network entity registry
			if (network_entity_scene.is_valid()) {
				Node *root_node = network_entity_scene->instantiate();
				if(root_node->get_class_name() == "NetworkEntity"){
					NetworkEntityInfo info{};
					info.id = newEntityId;
					info.name = file_name.get_basename();
					info.scene = network_entity_scene;

					//Register entity info
					m_networkEntityRegistry[newEntityId] = info;

					//Increment new entity id for future entities
					newEntityId++;
				}
				root_node->queue_free();
			}
		}
		file_name = dir->_get_next();
	}

	dir->list_dir_end();

	return true;
}

World *GDNet::get_world_singleton() {
	return  world;
}

void GDNet::register_zone(Zone *zone) {
	ZoneInfo zoneInfo{};
	zoneInfo.id = m_zoneIDCounter;
	zoneInfo.name = zone->get_name();
	zoneInfo.zone = zone;

	m_zoneRegistry[zoneInfo.id] = zoneInfo;
	m_zoneIDCounter++;
}

void GDNet::unregister_zone(Zone *zone) {
	m_zoneRegistry.erase(zone->m_zoneId);
	zone->m_zoneId = 0;
}

void GDNet::init_gdnet() {
	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
		ERR_FAIL_MSG("Could not initialize GameNetworkingSockets!");
		return;
	}

	if (!register_network_entities()) {
		ERR_FAIL_MSG("Could not load network entities!");
		return;
	}


	m_isInitialized = true;
	print_line("GDNet has been initialized!");
}

void GDNet::shutdown_gdnet() {
	GameNetworkingSockets_Kill();

	m_isInitialized = false;
}

bool GDNet::is_client() {
	return m_isClient;
}

bool GDNet::is_server() {
	return m_isServer;
}

bool GDNet::zone_exists(ZoneID_t zoneId) {
	return m_zoneRegistry.find(zoneId) != m_zoneRegistry.end();
}

bool GDNet::entity_exists(EntityID_t entityId) {
	return m_networkEntityRegistry.find(entityId) != m_networkEntityRegistry.end();
}

EntityID_t GDNet::get_entity_id_by_name(String entityName) {
	std::map<EntityID_t, NetworkEntityInfo>::iterator it;

	for(it = m_networkEntityRegistry.begin(); it != m_networkEntityRegistry.end(); ++it){
		if(it->second.name == entityName){
			return it->first;
		}
	}

	return 0;
}



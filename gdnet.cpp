#include "gdnet.h"
#include "core/io/dir_access.h"
#include "include/steam/steamnetworkingsockets.h"

Ref<GDNet> GDNet::s_singleton = nullptr;

GDNet::GDNet() {
	s_singleton = Ref<GDNet>(this);
	m_isInitialized = false;
	m_isClient = false;
	m_isServer = false;
}

GDNet::~GDNet() {}

void GDNet::_bind_methods() {
	ClassDB::bind_method("init_gdnet", &GDNet::init_gdnet);
	ClassDB::bind_method("shutdown_gdnet", &GDNet::shutdown_gdnet);
}

Ref<GDNet> GDNet::get_singleton() {
	return s_singleton;
}

bool GDNet::load_network_entities() {
	String path = "res://NetworkEntities/";

	Error openErr = OK;
	//Try opening the "NetworkEntities" special directory.
	Ref<DirAccess> dir = DirAccess::open(path, &openErr);

	if (openErr != OK) {
		ERR_PRINT("Failed to find the 'NetworkEntities' directory!");
		return false;
	}

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

		if (file_name.get_extension() == "tscn" || file_name.get_extension() == "scn") {
			String full_path = path.path_join(file_name);

			Ref<PackedScene> network_entity_scene = ResourceLoader::load(full_path);

			if (network_entity_scene.is_valid()) {
				Node *root_node = network_entity_scene->instantiate();
				print_line(root_node->get_class_name());
				root_node->queue_free();
			}
		}

		file_name = dir->_get_next();
	}

	dir->list_dir_end();

	return true;
}

void GDNet::init_gdnet() {
	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
		ERR_FAIL_MSG("Could not initialize GameNetworkingSockets!");
		return;
	}

	if (!load_network_entities()) {
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

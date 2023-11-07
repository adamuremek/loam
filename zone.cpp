#include "gdnet.h"


//===============Zone Implementation===============//

ZoneID_t Zone::m_zone_id_counter = 0;

Zone::Zone() {
	m_parentWorld = nullptr;
	m_zoneInstance = nullptr;
	m_instantiated = false;
	m_zoneId = 0;
}

Zone::~Zone() {
	if (m_parentWorld) {
		m_parentWorld->unregister_zone(this);
	}
	
	m_parentWorld = nullptr;
	m_zoneInstance = nullptr;
}

//==Protected Methods==//

void Zone::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_zone_scene"), &Zone::get_zone_scene);
	ClassDB::bind_method(D_METHOD("set_zone_scene", "zone_scene"), &Zone::set_zone_scene);
	ClassDB::bind_method(D_METHOD("instantiate_zone"), &Zone::instantiate_zone);

	//Expose zone scene property to be set in the inspector
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "Zone Scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_zone_scene", "get_zone_scene");
}

void Zone::_notification(int n_type) {
	switch (n_type) {
		case NOTIFICATION_ENTER_TREE: {
			m_zoneId = m_zone_id_counter;
			m_zone_id_counter++;

			Node *parent = get_parent();
			World *parentWorld = Object::cast_to<World>(parent);
			if (parentWorld) {
				m_parentWorld = parentWorld;
				m_parentWorld->register_zone(this);
				print_line(vformat("Registered zone with id %d.", m_zoneId));
			} else {
				print_line("WARNING: Zones should be parented to World nodes!");
			}
			break;
		}
	}
}

//==Public Methods==//

Ref<PackedScene> Zone::get_zone_scene() const {
	return m_zoneScene;
}

void Zone::set_zone_scene(const Ref<PackedScene>& zoneScene) {
	m_zoneScene = zoneScene;
}

ZoneID_t Zone::get_zone_id() const {
	return m_zoneId;
}

void Zone::instantiate_zone() {
	if (m_instantiated) {
		return;
	}

	m_zoneInstance = m_zoneScene->instantiate();
	call_deferred("add_child", m_zoneInstance);
	m_instantiated = true;
}

void Zone::add_player(PlayerID_t playerID) {
	m_playersInZone.push_back(playerID);
}

void Zone::create_network_entity(String entityName) {
	EntityID_t entityId;
	std::map<String, EntityID_t>::iterator it = GDNet::get_singleton()->m_entityIdByName.find(entityName);

	if (it != GDNet::get_singleton()->m_entityIdByName.end()) {

	} else {
		ERR_PRINT(vformat("Could not find entity with name %s!", entityName));
		return;
	}

	if (GDNet::get_singleton()->is_server()) {

	} else if (GDNet::get_singleton()->is_client()) {
		ISteamNetworkingMessage *pCreateEntityRequest = create_small_message(INSTANTIATE_NETWORK_ENTITY_REQUEST, m_zoneId, entityId, GDNet::get_singleton()->m_player.get_world_connection());
		send_message(GDNet::get_singleton()->m_player.get_world_connection(), pCreateEntityRequest);
	}
}

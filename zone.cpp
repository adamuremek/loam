#include "gdnet.h"

//===============Zone Implementation===============//
Zone::Zone() {}

Zone::~Zone() {}

//==Protected Methods==//

void Zone::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_zone_scene"), &Zone::get_zone_scene);
	ClassDB::bind_method(D_METHOD("set_zone_scene", "zone_scene"), &Zone::set_zone_scene);
	ClassDB::bind_method(D_METHOD("instantiate_zone"), &Zone::instantiate_zone);

	//Expose zone scene property to be set in the inspector
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "Zone Scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_zone_scene", "get_zone_scene");
}

void Zone::_notification(int n_type) {
	if(Engine::get_singleton()->is_editor_hint()){
		return;
	}

	switch (n_type) {
		case NOTIFICATION_ENTER_TREE: {
			GDNet::singleton->register_zone(this);
			break;
		}
		case NOTIFICATION_EXIT_TREE: {
			GDNet::singleton->unregister_zone(this);
			break;
		}
	}
}

//==Public Methods==//

Ref<PackedScene> Zone::get_zone_scene() const {
	return m_zoneScene;
}

void Zone::set_zone_scene(const Ref<PackedScene> &zoneScene) {
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
	//This is using the built in "add_child" mehtod to append the instance to the zone.
	call_deferred("add_child", m_zoneInstance);
	m_instantiated = true;
}

void Zone::add_player(PlayerID_t playerId) {
	m_playersInZone.push_back(playerId);
}

bool Zone::player_in_zone(PlayerID_t playerId) {
	return std::find(m_playersInZone.begin(), m_playersInZone.end(), playerId) != m_playersInZone.end();
}

void Zone::instantiate_network_entity(EntityID_t entityId, String parentNode) {
//	//Try to find the requested parent node
//	Node* parent = this->get_node(parentNode);
//
//	//Error and return if the requested parent node could not be found
//	if(parent == nullptr){
//		ERR_PRINT(vformat("Requested parent ndoe [%s] does not exist!", parentNode));
//		return;
//	}else{
//		//Instantiate the entity and
//		Ref<PackedScene> requestedEntity = GDNet::get_singleton()->m_entitiesById[entityId];
//		Node *entityInstance = requestedEntity->instantiate();
//		parent->add_child(entityInstance);
//	}
}



void Zone::create_entity(Ref<EntityInfo> entityInfo) {
	//Make sure a world is being hosted or a world is connected to before trying to instantiate entities.
	if(!GDNet::singleton->is_client() || !GDNet::singleton->is_server()){
		ERR_PRINT("Cannot create an entity if not hosting or connected to a world!");
		return;
	}

	//Make sure zone has been instantiated before calling
	if(!m_instantiated){
		ERR_PRINT("Cannot create entity in zone, zone has not been instantiated!");
		return;
	}

	//If an associated player is defined, make sure they are in the zone.
	if(entityInfo->m_entityInfo.associatedPlayer > 0 && !player_in_zone(entityInfo->m_entityInfo.associatedPlayer)){
		ERR_PRINT(vformat("Cannot associate entity with player \"ID: %d\". They are not in this zone!", entityInfo->m_entityInfo.associatedPlayer));
		return;
	}

	//Make sure all other user entered data is valid
	if(!entityInfo->verify_info()){
		ERR_PRINT("Cannot create entity, entity info is not valid!");
		return;
	}

	//Serialize the entity info
	entityInfo->serialize_info();
	unsigned char* data = entityInfo->m_entityInfo.dataBuffer.data();
	int dataLen = entityInfo->m_entityInfo.dataBuffer.size();


	if(GDNet::singleton->is_client()){
		//Make this message an entity creation request
		data[0] = CREATE_ENTITY_REQUEST;

		//Create and send the message
		SteamNetworkingMessage_t* mssg = allocate_message(data, dataLen, GDNet::singleton->world->m_worldConnection);
		send_message(mssg);
	}


}

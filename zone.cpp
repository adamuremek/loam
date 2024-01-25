#include "gdnet.h"

//===============Zone Implementation===============//
Zone::Zone() {
	m_zoneId = 0U;
	m_instantiated = false;
	m_zoneInstance = nullptr;
}

Zone::~Zone() {}

//==Protected Methods==//

void Zone::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_zone_scene"), &Zone::get_zone_scene);
	ClassDB::bind_method(D_METHOD("set_zone_scene", "zone_scene"), &Zone::set_zone_scene);
	ClassDB::bind_method(D_METHOD("instantiate_zone"), &Zone::instantiate_zone);
	ClassDB::bind_method(D_METHOD("load_entity", "entity_info"), &Zone::load_entity);

	ClassDB::bind_method(D_METHOD("instantiate_callback"), &Zone::instantiate_zone);
	ClassDB::bind_method(D_METHOD("player_loaded_callback", "player_info"), &Zone::player_loaded_callback);

	//Internal methods
	ClassDB::bind_method(D_METHOD("_remove_player", "player_info"), &Zone::remove_player);

	//Expose zone scene property to be set in the inspector
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "zone_scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_zone_scene", "get_zone_scene");

	ADD_SIGNAL(MethodInfo("player_loaded_zone", PropertyInfo(Variant::INT, "player_id")));
	ADD_SIGNAL(MethodInfo("player_left_zone", PropertyInfo(Variant::INT, "player_id")));

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

//Call this on the main thread
bool Zone::instantiate_zone() {
	//Just in case this method is somehow called when the zone is already instantiated
	if (m_instantiated) {
		return false;
	}

	//Make sure a packed scene was provided for the zone
	if(!m_zoneScene.is_valid()){
		ERR_PRINT(vformat("Zone with ID %d was not given a scene to create!", m_zoneId));
		return false;
	}
	//Instantiate the actual packed scene
	m_zoneInstance = m_zoneScene->instantiate();

	//Instantiate the actual packed scene
	m_zoneInstance = m_zoneScene->instantiate();

	//Indicate that the zone has been instantiated through the boolean flag
	//Gotta call this before adding the instance to the scene otherwise if any entities are
	//created, it will think that the zone doenst exist and throw an error.
	m_instantiated = true;

	//This is using the built in "add_child" mehtod to append the instance to the zone.
	add_child(m_zoneInstance);

	//Raise the "loaded_zone" signal
	GDNet::singleton->world->emit_signal("loaded_zone", this);

	return true;
}

void Zone::uninstantiate_zone() {
	//Destory all entities in the zone
	for(const KeyValue<EntityNetworkID_t, Ref<EntityInfo>> &entity : m_entitiesInZone){
		destroy_entity(entity.value);
	}

	//Clear all players from the zone
	m_playersInZone.clear();

	//Destroy the zone instance
	m_zoneInstance->queue_free();

	//Mark that the zone is no longer instantiated
	m_instantiated = false;
}

void Zone::add_player(Ref<PlayerInfo> playerInfo) {
	//Store the player info within the zone
	m_playersInZone.insert(playerInfo->get_player_id(), playerInfo);
	//Add this zone to the player's list of loaded zones
	playerInfo->set_current_loaded_zone(this);
	//Queue the signal emission
	call_deferred("player_loaded_callback", playerInfo);
}

void Zone::remove_player(Ref<PlayerInfo> playerInfo) {
	print_line("Starting player removal...");
	PlayerID_t playerId = playerInfo->get_player_id();

	if(!m_playersInZone.has(playerId)){
		ERR_PRINT(vformat("No player wint ID %d is in zone %d!", playerId, m_zoneId));
		return;
	}
	print_line("Killing entities...");
	//Iterate through the player's owned entities, and remove them
	//from the player's list and the zone if they exist in this zone
	print_line(vformat("number of entiteis owned: %d", playerInfo->m_playerInfo.ownedEntities.size()));
	for(const KeyValue<EntityNetworkID_t, Ref<EntityInfo>> &ownedEntity : playerInfo->m_playerInfo.ownedEntities){
		print_line(vformat("Killing entity: %d", ownedEntity.key));
		if(m_entitiesInZone.has(ownedEntity.key)){
			destroy_entity(ownedEntity.value);
		}
	}
	playerInfo->m_playerInfo.ownedEntities.clear();

	print_line("entities killed!");
	//Remove player from zone's player list (this has to be done after the above bc
	//the "destroy_entity" method uses the player list to erase the owned entity from the player)
	m_playersInZone.erase(playerId);

	//Emit the "player_left_zone" signal
	emit_signal("player_left_zone", playerId);

	//Reset the player's zone info
	print_line("cock");
	playerInfo->reset_zone_info();
	print_line("balls");
	print_line("Player removal finished!");
}

void Zone::load_entity(Ref<EntityInfo> entityInfo) {
	//Make sure a world is being hosted or a world is connected to before trying to instantiate entities.
	if(!GDNet::singleton->is_client() && !GDNet::singleton->is_server()){
		ERR_PRINT("Cannot create an entity if not hosting or connected to a world!");
		return;
	}

	//Make sure zone has been instantiated before calling
	if(!m_instantiated){
		ERR_PRINT("Cannot create entity in zone, zone has not been instantiated!");
		return;
	}

	//If an associated player is defined, make sure they are in the zone.
	if(entityInfo->get_owner_id() > 0 && !player_in_zone(entityInfo->get_owner_id())){
		ERR_PRINT(vformat("Cannot associate entity with player \"ID: %d\". They are not in this zone!", entityInfo->get_owner_id()));
		return;
	}

	//Make sure all other user entered data is valid
	if(!entityInfo->verify_info()){
		ERR_PRINT("Cannot create entity, entity info is not valid!");
		return;
	}

	//Set the entity's parent zone id
	entityInfo->m_entityInfo.parentZone = m_zoneId;

	if(GDNet::singleton->is_client()){
		//Serialize the entity info
		entityInfo->serialize_info();

		//Make the message a creation request and prepare the data
		entityInfo->m_entityInfo.dataBuffer.set(0, CREATE_ENTITY_REQUEST);
		const unsigned char* mssgData = entityInfo->m_entityInfo.dataBuffer.ptr();
		int mssgDataLen = entityInfo->m_entityInfo.dataBuffer.size();

		//Create and send the message
		SteamNetworkingMessage_t* mssg = allocate_message(mssgData, mssgDataLen, GDNet::singleton->world->m_worldConnection);
		send_message_reliable(mssg);
		print_line("Sent entity creation request! :)");
	}else if(GDNet::singleton->is_server()){
		//Assign a network id for the entity
		entityInfo->set_network_id(IDGenerator::generateNetworkIdentityID());
		//Create the entity
		create_entity(entityInfo);

		//Reserialize the entity info with the new data in it
		entityInfo->serialize_info();

		//Tell each player in the zone to also create this entity
		for (const KeyValue<PlayerID_t, Ref<PlayerInfo>> &player : m_playersInZone) {
			player.value->load_entity(entityInfo);
		}
	}
}

void Zone::create_entity(Ref<EntityInfo> entityInfo) {
	print_line("Creating Entity...");
	//Store local references to relevant objects
	EntityID_t entityId = entityInfo->get_entity_id();
	String parentRelativePath = entityInfo->get_parent_relative_path();
	PlayerID_t ownerId = entityInfo->get_owner_id();

	//Instantiate the entity
	Node* instance = GDNet::singleton->m_networkEntityRegistry.get(entityId).scene->instantiate();
	NetworkEntity* instanceAsEntity = Object::cast_to<NetworkEntity>(instance);
	entityInfo->m_entityInfo.entityInstance = instanceAsEntity;

	//Assign info reference to the instance
	instanceAsEntity->m_info = entityInfo;

	//Get a refrence to the requested parent node if one was provided. Otherwise just use the instance as the base node.
	Node *parentNode;
	if(parentRelativePath == ""){
		parentNode = m_zoneInstance;
	}else {
		parentNode = m_zoneInstance->get_node(parentRelativePath);
	}

	//Add the entity to the zone scene
	parentNode->call_deferred("add_child", instanceAsEntity);

	//Add the entity to list of known entities in zone
	m_entitiesInZone.insert(entityInfo->m_entityInfo.networkId, entityInfo);

	//Store the parent zone instance in the entity
	instanceAsEntity->m_parentZone = this;

	//Associate the entity with a player (if such a player was specified)
	if(ownerId != 0){
		print_line("gabba goo!");
		m_playersInZone.get(ownerId)->add_owned_entity(entityInfo);
		print_line("gooba gaa!");
	}

	//Connect the entity to data transmission signals
	if(GDNet::singleton->m_isServer){
		Callable transmitCallback = Callable(instanceAsEntity, "server_side_transmit_data");
		GDNet::singleton->world->connect("_server_side_transmit_entity_data", transmitCallback);
	}else{
		Callable transmitCallback = Callable(instanceAsEntity, "client_side_transmit_data");
		GDNet::singleton->world->connect("_client_side_transmit_entity_data", transmitCallback);
	}


	print_line("Entity Created!");
}

void Zone::destroy_entity(Ref<EntityInfo> entityInfo) {
	print_line("Entity destruction started....");
	//Make sure the provided entity exists in this zone
	EntityNetworkID_t networkId = entityInfo->get_network_id();
	if(!m_entitiesInZone.has(networkId)){
		ERR_PRINT(vformat("Entity with net id %d does not exist in this zone!", entityInfo->get_network_id()));
		return;
	}else{
		//Remove the entity reference stored in the zone
		m_entitiesInZone.erase(networkId);
	}

	//Disconnect the entity from data transmission signals
	NetworkEntity* instanceAsEntity = entityInfo->m_entityInfo.entityInstance;
	if(GDNet::singleton->m_isServer){
		Callable transmitCallback = Callable(instanceAsEntity, "server_side_transmit_data");
		GDNet::singleton->world->disconnect("_server_side_transmit_entity_data", transmitCallback);
	}else{
		Callable transmitCallback = Callable(instanceAsEntity, "client_side_transmit_data");
		GDNet::singleton->world->disconnect("_client_side_transmit_entity_data", transmitCallback);
	}

	//Unlink the zone from the entity
	instanceAsEntity->m_parentZone = nullptr;

	//Delete the node instance
	instanceAsEntity->queue_free();

	//Remove the entity instance pointer reference from the entity info
	entityInfo->m_entityInfo.entityInstance = nullptr;

	print_line(vformat("Ref Count for entity %d: %d", networkId, entityInfo->get_reference_count()));

	print_line("Entity destoryed");
}

void Zone::player_loaded_callback(Ref<PlayerInfo> playerInfo) {
	//Emit the "player_loaded_zone" signal
	emit_signal("player_loaded_zone", playerInfo->get_player_id());
}


bool Zone::player_in_zone(PlayerID_t playerId) {
	return m_playersInZone.has(playerId);
}

bool Zone::is_instantiated() {
	return m_instantiated;
}


Ref<PackedScene> Zone::get_zone_scene() const {
	return m_zoneScene;
}

Ref<PlayerInfo> Zone::get_player(PlayerID_t playerId) const {
	if(m_playersInZone.has(playerId)){
		return m_playersInZone.get(playerId);
	}else{
		return nullptr;
	}
}

ZoneID_t Zone::get_zone_id() const {
	return m_zoneId;
}


void Zone::set_zone_scene(const Ref<PackedScene> &zoneScene) {
	m_zoneScene = zoneScene;
}

void Zone::set_zone_id(const ZoneID_t zoneId) {
	m_zoneId = zoneId;
}

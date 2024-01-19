#include "gdnet.h"

PlayerInfo::PlayerInfo(){
	m_playerInfo.loadedPlayersInZone = false;
	m_playerInfo.loadedEntitiesInZone = false;
	m_playerInfo.currentLoadedZone = nullptr;
}
PlayerInfo::~PlayerInfo(){}

void PlayerInfo::_bind_methods() {}

void PlayerInfo::load_player(Ref<PlayerInfo> playerInfo) {
	//Add player (player ID) to the ACK waiting buffer
	m_playerInfo.playersWaitingForLoadAck.push_back(playerInfo->get_player_id());

	//Create a mini message containing the player id
	SteamNetworkingMessage_t *playerRequestMssg = create_mini_message(CREATE_ZONE_PLAYER_INFO_REQUEST, playerInfo->get_player_id(), get_player_conn());

	//Send the message to the loading player
	send_message_reliable(playerRequestMssg);
}

void PlayerInfo::load_entity(Ref<EntityInfo> entityInfo) {
	//Reserialize the entity info
	entityInfo->serialize_info();

	//Make the message request entity creations
	entityInfo->m_entityInfo.dataBuffer.set(0,  CREATE_ENTITY_REQUEST);

	//Get message data
	const unsigned char* mssgData = entityInfo->m_entityInfo.dataBuffer.ptr();
	int dataLen = entityInfo->m_entityInfo.dataBuffer.size();

	//Add the entity (network id) to the ACK waiting buffer
	m_playerInfo.entitiesWaitingForLoadAck.push_back(entityInfo->m_entityInfo.networkId);

	//Create and send the message
	SteamNetworkingMessage_t *createMssg = allocate_message(mssgData, dataLen, get_player_conn());
	send_message_reliable(createMssg);
}

void PlayerInfo::load_entities_in_current_zone() {
	// Make the player start loading all entities in the zone by intitiating the first entity load.
	// This will start a chain of requests that eventually loads all entities on the player's end.
	for(const KeyValue<EntityNetworkID_t, Ref<EntityInfo>> &element : get_current_loaded_zone()->m_entitiesInZone){
		load_entity(element.value);
	}
}

void PlayerInfo::add_owned_entity(Ref<EntityInfo> associatedEntity) {
	m_playerInfo.ownedEntities.insert(associatedEntity->get_network_id(),associatedEntity);
}

void PlayerInfo::confirm_player_load(PlayerID_t playerId) {
	print_line(vformat("CURRENT PLAYER: %d -- PLAYER THAT WAS LOADED: %d", get_player_id() ,playerId));
	print_line(vformat("ACK waiting in buffer: %d", m_playerInfo.playersWaitingForLoadAck.size()));
	//Remove the player from the ACK buffer
	m_playerInfo.playersWaitingForLoadAck.erase(playerId);
	print_line(vformat("ACK waiting in buffer after removal: %d", m_playerInfo.playersWaitingForLoadAck.size()));
	String option = m_playerInfo.loadedPlayersInZone ? "True" : "False";
	print_line(vformat("LoadedPlayersInZoneBool: %s", option));

	//Once the ACK buffer is empty and the player has made player info copies for all players in the zone,
	//start loading in all the entities in the zone
	if(m_playerInfo.playersWaitingForLoadAck.size() == 0 && !m_playerInfo.loadedPlayersInZone){
		print_line(vformat("NEW: Starting to load all entites in zone for player %d!", get_player_id()));
		//Mark that the player has loaded all other players in the zone locally
		m_playerInfo.loadedPlayersInZone = true;

		//Load all entiteis in the zone
		load_entities_in_current_zone();
	}
}

void PlayerInfo::confirm_entity_load(EntityNetworkID_t entityNetworkId) {
	//Remove the entity from the ACK buffer
	m_playerInfo.entitiesWaitingForLoadAck.erase(entityNetworkId);

	if(m_playerInfo.entitiesWaitingForLoadAck.size() == 0 && !m_playerInfo.loadedEntitiesInZone){
		//Mark that this player has loaded all entities in the zone
		m_playerInfo.loadedEntitiesInZone = true;

		// Inform all players in the zone that this player has fully loaded into the zone
		for (const KeyValue<PlayerID_t, Ref<PlayerInfo>> &player : m_playerInfo.currentLoadedZone->m_playersInZone) {
			HSteamNetConnection destination = player.value->get_player_conn();
			SteamNetworkingMessage_t *playerEnteredZoneMssg = create_mini_message(LOAD_ZONE_COMPLETE, get_player_id(), destination);
			send_message_reliable(playerEnteredZoneMssg);
		}
	}
}

void PlayerInfo::reset_zone_info() {
	m_playerInfo.currentLoadedZone = nullptr;
	m_playerInfo.ownedEntities.clear();
	m_playerInfo.loadedPlayersInZone = false;
	m_playerInfo.loadedEntitiesInZone = false;
	m_playerInfo.entitiesWaitingForLoadAck.clear();
	m_playerInfo.playersWaitingForLoadAck.clear();
}


PlayerID_t PlayerInfo::get_player_id() {
	return m_playerInfo.id;
}

HSteamNetConnection PlayerInfo::get_player_conn() {
	return m_playerInfo.playerConnection;
}

Zone *PlayerInfo::get_current_loaded_zone() {
	return m_playerInfo.currentLoadedZone;
}


void PlayerInfo::set_player_id(PlayerID_t playerId) {
	m_playerInfo.id = playerId;
}

void PlayerInfo::set_player_conn(HSteamNetConnection playerConnection) {
	m_playerInfo.playerConnection = playerConnection;
}

void PlayerInfo::set_current_loaded_zone(Zone *zone) {
	m_playerInfo.currentLoadedZone = zone;
}

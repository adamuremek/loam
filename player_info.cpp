#include "gdnet.h"

PlayerInfo::PlayerInfo(){}
PlayerInfo::~PlayerInfo(){}

void PlayerInfo::_bind_methods() {

}

void PlayerInfo::load_entity(Ref<EntityInfo> entityInfo) {
	//Make the message request entity creations
	entityInfo->m_entityInfo.dataBuffer.set(0,  CREATE_ENTITY_REQUEST);

	//Get message data
	const unsigned char* mssgData = entityInfo->m_entityInfo.dataBuffer.ptr();
	int dataLen = entityInfo->m_entityInfo.dataBuffer.size();

	//Add the entity (network id) to the ACK waiting buffer
	m_playerInfo.entitiesWaitingForAck.push_back(entityInfo->m_entityInfo.networkId);

	//Create and send the message
	SteamNetworkingMessage_t *createMssg = allocate_message(mssgData, dataLen, get_player_conn());
	send_message_reliable(createMssg);
}

void PlayerInfo::add_associated_entity(Ref<EntityInfo> associatedEntity) {
	m_playerInfo.associatedEntities.push_back(associatedEntity);
}

void PlayerInfo::confirm_entity(EntityNetworkID_t entityNetworkId) {
	m_playerInfo.entitiesWaitingForAck.erase(entityNetworkId);

	if(m_playerInfo.entitiesWaitingForAck.size() == 0 && !m_playerInfo.zoneLoadComplete){
		//Mark that this player is now loaded into the zone
		m_playerInfo.zoneLoadComplete = true;

		//Send a zone load complete message to this player
		SteamNetworkingMessage_t *loadCompleteMssg = create_mini_message(LOAD_ZONE_COMPLETE, 0, m_playerInfo.playerConnection);
		send_message_reliable(loadCompleteMssg);

		// Inform all (other) players in the zone that this player has fully loaded into the zone
		for (const Ref<PlayerInfo> &player : m_playerInfo.currentLoadedZone->m_playersInZone) {
			if(player == this){
				print_line("Skipped current player! :)");
				continue;
			}

			HSteamNetConnection destination = player->get_player_conn();
			SteamNetworkingMessage_t *playerEnteredZoneMssg = create_mini_message(PLAYER_ENTERED_ZONE, get_player_id(), destination);
			send_message_reliable(playerEnteredZoneMssg);
		}
	}
}


PlayerID_t PlayerInfo::get_player_id() {
	return m_playerInfo.id;
}

Zone *PlayerInfo::get_current_loaded_zone() {
	return m_playerInfo.currentLoadedZone;
}

HSteamNetConnection PlayerInfo::get_player_conn() {
	return m_playerInfo.playerConnection;
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

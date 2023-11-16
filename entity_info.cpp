#include "gdnet.h"

EntityInfo::EntityInfo() :
		m_entityInfo{} {}

EntityInfo::~EntityInfo() {}

void EntityInfo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_entity_name", "name"), &EntityInfo::set_entity_name);
	ClassDB::bind_method(D_METHOD("set_entity_id", "id"), &EntityInfo::set_entity_id);
	ClassDB::bind_method(D_METHOD("set_associated_player_id", "associatedPlayer"), &EntityInfo::set_associated_player_id);
	ClassDB::bind_method(D_METHOD("get_entity_name"), &EntityInfo::get_entity_name);
	ClassDB::bind_method(D_METHOD("get_entity_id"), &EntityInfo::get_entity_id);
	ClassDB::bind_method(D_METHOD("get_associated_player_id"), &EntityInfo::get_associated_player_id);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_entity_name", "get_entity_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "entity_id"), "set_entity_id", "get_entity_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "associated_player"), "set_associated_player_id", "get_associated_player_id");
}

bool EntityInfo::verify_info() {
	EntityID_t entityFoundByName;
	bool entityFoundById;

	// Try to find the entity in the registry by name
	entityFoundByName = GDNet::singleton->get_entity_id_by_name(m_entityInfo.entityName);

	// Check to see if the provided entity id exists in the registry
	entityFoundById = GDNet::singleton->entity_exists(m_entityInfo.entityId);

	// If the entity could not be found by name or id, print error and return
	if (!entityFoundByName && !entityFoundById) {
		ERR_PRINT(vformat("No entity with name \"%s\" or ID \"%d\" could be found in the registry!", m_entityInfo.entityName, m_entityInfo.entityId));
		return false;
	}else{
		// If a valid name was found first, then set m_entityInfo's current id to the corresponding id for consistency.
		// If a valid id was found first, then similary change the current name to the corresponding name for consistency.
		if(entityFoundByName > 0){
			m_entityInfo.entityId = entityFoundByName;
		}else{
			m_entityInfo.entityName = GDNet::singleton->m_networkEntityRegistry[m_entityInfo.entityId].name;
		}
	}

	// Make sure the player exists somewhere in the world if one was provided
	if(m_entityInfo.associatedPlayer > 0 && !GDNet::singleton->world->player_exists(m_entityInfo.associatedPlayer)){
		ERR_PRINT(vformat("No player with id \"%d\" exists in this current world!", m_entityInfo.associatedPlayer));
		return false;
	}

	return true;
}

void EntityInfo::serialize_info() {
	//Clear the buffer in case
	m_entityInfo.dataBuffer.clear();

	//Size the data buffer
	int nameLen = m_entityInfo.entityName.length();
	int bufferSize = 1 + nameLen + sizeof(EntityID_t) + sizeof(PlayerID_t);
	m_entityInfo.dataBuffer.resize(bufferSize);

	int bufferIdx = 0;

	//Add an empty request type as a placeholder
	m_entityInfo.dataBuffer.push_back(0);
	bufferIdx += 1;

	//Add the string length of the entity name to the buffer
	serialize_int(nameLen, bufferIdx, m_entityInfo.dataBuffer.data());
	bufferIdx += sizeof(int);

	//Add the string to the buffer
	const char* nameData = m_entityInfo.entityName.utf8().get_data();
	for(int i = 0; i < nameLen; i++){
		m_entityInfo.dataBuffer.push_back(static_cast<unsigned int>(nameData[i]));
	}
	bufferIdx += nameLen;

	//Add the entity id to the buffer
	serialize_uint(m_entityInfo.entityId, bufferIdx, m_entityInfo.dataBuffer.data());
	bufferIdx += sizeof(EntityID_t);

	//Add the associated player id to the buffer
	serialize_uint(m_entityInfo.associatedPlayer, bufferIdx, m_entityInfo.dataBuffer.data());
}

void EntityInfo::set_entity_name(String name) {
	m_entityInfo.entityName = name;
}

void EntityInfo::set_entity_id(EntityID_t id) {
	m_entityInfo.entityId = id;
}

void EntityInfo::set_associated_player_id(PlayerID_t associatedPlayer) {
	m_entityInfo.associatedPlayer = associatedPlayer;
}

String EntityInfo::get_entity_name() {
	return m_entityInfo.entityName;
}

EntityID_t EntityInfo::get_entity_id() {
	return m_entityInfo.entityId;
}

PlayerID_t EntityInfo::get_associated_player_id() {
	return m_entityInfo.associatedPlayer;
}
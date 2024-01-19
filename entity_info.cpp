#include "gdnet.h"

EntityInfo::EntityInfo() :
		m_entityInfo{} {}

EntityInfo::~EntityInfo() {}

void EntityInfo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_entity_name"), &EntityInfo::get_entity_name);
	ClassDB::bind_method(D_METHOD("get_parent_relative_path"), &EntityInfo::get_parent_relative_path);
	ClassDB::bind_method(D_METHOD("get_entity_id"), &EntityInfo::get_entity_id);
	ClassDB::bind_method(D_METHOD("get_owner_id"), &EntityInfo::get_owner_id);
	ClassDB::bind_method(D_METHOD("get_initial_position_3d"), &EntityInfo::get_initial_position_3D);
	ClassDB::bind_method(D_METHOD("get_initial_position_2d"), &EntityInfo::get_initial_position_2D);

	ClassDB::bind_method(D_METHOD("set_entity_name", "name"), &EntityInfo::set_entity_name);
	ClassDB::bind_method(D_METHOD("set_parent_relative_path", "path"), &EntityInfo::set_parent_relative_path);
	ClassDB::bind_method(D_METHOD("set_entity_id", "id"), &EntityInfo::set_entity_id);
	ClassDB::bind_method(D_METHOD("set_owner_id", "associatedPlayer"), &EntityInfo::set_owner_id);
	ClassDB::bind_method(D_METHOD("set_initial_position_3d", "position"), &EntityInfo::set_initial_position_3D);
	ClassDB::bind_method(D_METHOD("set_initial_position_2d", "position"), &EntityInfo::set_initial_position_2D);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_entity_name", "get_entity_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "parentRelativePath"), "set_parent_relative_path", "get_parent_relative_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "entity_id"), "set_entity_id", "get_entity_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "owner_id"), "set_owner_id", "get_owner_id");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "initial_position_3d", PROPERTY_HINT_RANGE, "-99999,99999,0.001,or_greater,or_less,hide_slider,suffix:m", PROPERTY_USAGE_EDITOR), "set_initial_position_3d", "get_initial_position_3d");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "initial_position_2d", PROPERTY_HINT_RANGE, "-99999,99999,0.001,or_greater,or_less,hide_slider,suffix:m", PROPERTY_USAGE_EDITOR), "set_initial_position_2d", "get_initial_position_2d");

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
	if(m_entityInfo.owner > 0 && !GDNet::singleton->world->player_exists(m_entityInfo.owner)){
		ERR_PRINT(vformat("No player with id \"%d\" exists in this current world!", m_entityInfo.owner));
		return false;
	}

	return true;
}

void EntityInfo::serialize_info() {
	//Clear the buffer in case
	m_entityInfo.dataBuffer.clear();

	//Get name and path char stirngs and lengths
	CharString name = m_entityInfo.entityName.utf8();
	CharString path = m_entityInfo.parentRelativePath.utf8();
	int nameLen = name.size();
	int pathLen = path.size();

	//Size the data buffer
	// request type + name char len + name + path char len + path + parent zone id
	// + entity id + network id + ass. player id
	int bufferSize = 1 + (6 * sizeof(uint32_t)) + nameLen + pathLen + sizeof(Vector3) + sizeof(Vector2);
	m_entityInfo.dataBuffer.resize(bufferSize);

	//Start an idx counter and keep note of 32 bit integer size (it should almost always be 4 bytes, idk why I did this)
	int bufferIdx = 0;
	std::size_t numericSize = sizeof(uint32_t);

	//Add an empty request type as a placeholder
	m_entityInfo.dataBuffer.set(0, 0);
	bufferIdx += 1;

	//Add the string length of the entity name to the buffer
	serialize_int(nameLen, bufferIdx, m_entityInfo.dataBuffer);
	bufferIdx += numericSize;

	//Add the name string to the buffer
	const char* nameData = name.get_data();
	for(int i = 0; i < nameLen; i++){
		m_entityInfo.dataBuffer.set(bufferIdx + i, static_cast<unsigned char>(nameData[i]));
	}
	bufferIdx += nameLen;

	//Add the string lenthg of the entity relative path to the buffer
	serialize_int(pathLen, bufferIdx, m_entityInfo.dataBuffer);
	bufferIdx += numericSize;

	//Add the path string to the buffer
	const char* pathData = path.get_data();
	for(int i = 0; i < pathLen; i++){
		m_entityInfo.dataBuffer.set(bufferIdx + i, static_cast<unsigned char>(pathData[i]));
	}
	bufferIdx += pathLen;

	//Add the parent zone id to the buffer
	serialize_uint(m_entityInfo.parentZone, bufferIdx, m_entityInfo.dataBuffer);
	bufferIdx += numericSize;

	//Add the entity id to the buffer
	serialize_uint(m_entityInfo.entityId, bufferIdx, m_entityInfo.dataBuffer);
	bufferIdx += numericSize;

	//Add the network id to the buffer
	serialize_uint(m_entityInfo.networkId, bufferIdx, m_entityInfo.dataBuffer);
	bufferIdx += numericSize;

	//Add the associated player id to the buffer
	serialize_uint(m_entityInfo.owner, bufferIdx, m_entityInfo.dataBuffer);
	bufferIdx += numericSize;

	//Add the initial position 3D to the buffer
	serialize_basic(m_entityInfo.initialPosition3D, bufferIdx, m_entityInfo.dataBuffer);
	bufferIdx += sizeof(Vector3);

	//Add the initial position 2D to the buffer
	serialize_basic(m_entityInfo.initialPosition2D, bufferIdx, m_entityInfo.dataBuffer);
}

void EntityInfo::deserialize_info(const unsigned char *data) {
	//Clear the buffer
	m_entityInfo.dataBuffer.clear();
	int bufferIdx = 1;
	std::size_t numericSize = sizeof(uint32_t);

	//Get the entity name
	int nameLen = deserialize_int(bufferIdx, data);
	bufferIdx += numericSize;
	m_entityInfo.entityName = deserialize_string(bufferIdx, nameLen, data);
	bufferIdx += nameLen;

	//Get the parent relative path
	int pathLen = deserialize_int(bufferIdx, data);
	bufferIdx += numericSize;
	m_entityInfo.parentRelativePath = deserialize_string(bufferIdx, pathLen, data);
	bufferIdx += pathLen;

	//Get the parent zone id
	m_entityInfo.parentZone = deserialize_uint(bufferIdx, data);
	bufferIdx += numericSize;

	//Get the entity id
	m_entityInfo.entityId = deserialize_uint(bufferIdx, data);
	bufferIdx += numericSize;

	//Get the entity's network id
	m_entityInfo.networkId = deserialize_uint(bufferIdx, data);
	bufferIdx += numericSize;

	//Get the owner id
	m_entityInfo.owner = deserialize_uint(bufferIdx, data);
	bufferIdx += numericSize;

	//Get the initial position 3D
	m_entityInfo.initialPosition3D = deserialize_basic<Vector3>(bufferIdx, data);
	bufferIdx += sizeof(Vector3);

	//Get the initial position 2D
	m_entityInfo.initialPosition2D = deserialize_basic<Vector2>(bufferIdx, data);
}

//==================GETTERS AND SETTERS==================//

String EntityInfo::get_entity_name() {
	return m_entityInfo.entityName;
}

String EntityInfo::get_parent_relative_path() {
	return m_entityInfo.parentRelativePath;
}

EntityID_t EntityInfo::get_entity_id() {
	return m_entityInfo.entityId;
}

PlayerID_t EntityInfo::get_owner_id() {
	return m_entityInfo.owner;
}

EntityNetworkID_t EntityInfo::get_network_id() {
	return m_entityInfo.networkId;
}

Vector3 EntityInfo::get_initial_position_3D() {
	return m_entityInfo.initialPosition3D;
}

Vector2 EntityInfo::get_initial_position_2D() {
	return m_entityInfo.initialPosition2D;
}


void EntityInfo::set_entity_name(String name) {
	m_entityInfo.entityName = name;
}

void EntityInfo::set_parent_relative_path(String path) {
	m_entityInfo.parentRelativePath = path;
}

void EntityInfo::set_entity_id(EntityID_t id) {
	m_entityInfo.entityId = id;
}

void EntityInfo::set_owner_id(PlayerID_t ownerId) {
	m_entityInfo.owner = ownerId;
}

void EntityInfo::set_network_id(EntityNetworkID_t networkId) {
	m_entityInfo.networkId = networkId;
}

void EntityInfo::set_initial_position_3D(Vector3 pos) {
	m_entityInfo.initialPosition3D = pos;
}

void EntityInfo::set_initial_position_2D(Vector2 pos) {
	m_entityInfo.initialPosition2D = pos;
}

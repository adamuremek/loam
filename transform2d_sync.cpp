#include "gdnet.h"

Transform2DSync::Transform2DSync() {
	m_interpolate = false;
	m_target = nullptr;
	m_interpolationTime = 0.1f;
	m_elapsedTime = 0.0f;
	m_authority = SyncAuthority::NONE;
}

void Transform2DSync::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_interpolate"), &Transform2DSync::get_interpolate);
	ClassDB::bind_method(D_METHOD("get_target"), &Transform2DSync::get_target);
	ClassDB::bind_method(D_METHOD("get_position"), &Transform2DSync::get_position);
	ClassDB::bind_method(D_METHOD("get_authority"), &Transform2DSync::get_authority);

	ClassDB::bind_method(D_METHOD("set_interpolate", "interpolate"), &Transform2DSync::set_interpolate);
	ClassDB::bind_method(D_METHOD("set_target", "target"), &Transform2DSync::set_target);
	ClassDB::bind_method(D_METHOD("set_position", "position"), &Transform2DSync::set_position);
	ClassDB::bind_method(D_METHOD("set_authority", "authority"), &Transform2DSync::set_authority);

	ClassDB::bind_method(D_METHOD("update_transform_data"), &Transform2DSync::update_transform_data);
	ClassDB::bind_method(D_METHOD("copy_transform"), &Transform2DSync::copy_transform);

	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(OWNER_AUTHORITATIVE);
	BIND_ENUM_CONSTANT(SERVER_AUTHORITATIVE);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "interpolate"), "set_interpolate", "get_interpolate");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "target", PROPERTY_HINT_RESOURCE_TYPE, "Node2D"), "set_target", "get_target");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position", PROPERTY_HINT_RANGE, "-99999,99999,0.001,or_greater,or_less,hide_slider,suffix:m", PROPERTY_USAGE_EDITOR), "set_position", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "authority", PROPERTY_HINT_ENUM, "NONE, OWNER_AUTHORITATIVE", PROPERTY_USAGE_DEFAULT), "set_authority", "get_authority");
}

void Transform2DSync::serialize_payload(EntityUpdateInfo_t &updateInfo) {
	//Resize the data buffer to accomodate the transform information
	updateInfo.dataBuffer.resize(METADATA_SIZE + sizeof(Transform2D));

	//Serialize the target transform into the message data
	serialize_basic(global_transform, METADATA_SIZE, updateInfo.dataBuffer);
}

void Transform2DSync::deserialize_payload(const EntityUpdateInfo_t &updateInfo) {
	//Get the target transform from the payload
	global_transform = deserialize_basic<Transform2D>(METADATA_SIZE, updateInfo.dataBuffer.ptr());
}

void Transform2DSync::tick() {
	m_tickCount %= m_maxTickCount;

	if(m_tickCount == 0){
		if(GDNet::singleton->m_isServer){
			for(const KeyValue<PlayerID_t, Ref<PlayerInfo>> &player : m_parentNetworkEntity->m_parentZone->m_playersInZone){
				transmit_data(player.value->get_player_conn());
			}
		}else if(GDNet::singleton->m_isClient && has_authority()){
			transmit_data(GDNet::singleton->world->m_worldConnection);
		}
	}

	m_tickCount++;
}

void Transform2DSync::transmit_data(HSteamNetConnection destination) {
	if(!m_target){
		return;
	}

	//Create and populate the update info
	EntityUpdateInfo_t updateInfo;

	updateInfo.parentZone = m_parentNetworkEntity->m_info->m_entityInfo.parentZone;
	updateInfo.networkId = m_parentNetworkEntity->m_info->m_entityInfo.networkId;
	updateInfo.updateType = TRANSFORM2D_SYNC_UPDATE;

	//Serialize the update info with pertinent information
	//Metadata
	NetworkModule::serialize_update_metadata(updateInfo);
	//Transform info
	serialize_payload(updateInfo);

	//Create and send the message to the destination.
	const unsigned char* mssgData = updateInfo.dataBuffer.ptr();
	int dataLen = updateInfo.dataBuffer.size();

	SteamNetworkingMessage_t *updateMssg = allocate_message(mssgData, dataLen, destination);
	send_message_unreliable(updateMssg);
}

void Transform2DSync::recieve_data(EntityUpdateInfo_t updateInfo) {
	//Obtain and store the transform information within the class
	deserialize_payload(updateInfo);

	if(GDNet::singleton->m_isServer){
		//Reset initial position
		m_parentNetworkEntity->m_info->set_initial_position_2D(global_transform.get_origin());
		//Set the transform
		m_target->call_deferred("set_transform", global_transform);
	}else if(GDNet::singleton->m_isClient){
		call_deferred("update_transform_data");
	}
}

//Must be called from main thread only
void Transform2DSync::interpolate_origin(float delta) {
	if(m_interpolate){
		m_elapsedTime += delta;
		float t = m_elapsedTime / m_interpolationTime;
		Vector2 interpolatedPosition = m_currentPosition.lerp(m_targetPosition, t);
		m_target->set_position(interpolatedPosition);
	}
	else{
		m_target->set_transform(global_transform);
	}

}

bool Transform2DSync::has_authority() {
	switch(m_authority){
		case SyncAuthority::NONE:{
			return false;
		}
		case SyncAuthority::OWNER_AUTHORITATIVE:{
			return m_parentNetworkEntity->has_ownership();
		}
		case SyncAuthority::SERVER_AUTHORITATIVE:{
			return GDNet::singleton->m_isServer;
		}
		default:
			return false;
	}
}

bool Transform2DSync::has_target() {
	return m_target;
}

void Transform2DSync::update_transform_data() {
	//m_target->set_transform(global_transform);
	m_currentPosition = m_target->get_position();
	m_targetPosition = global_transform.get_origin();
	m_elapsedTime = 0.0f;
}

//This method should only ever be called from the main thread
void Transform2DSync::copy_transform() {
	if(!m_target){
		return;
	}

	//Make sure caller has ownership of this entity
	if(!m_parentNetworkEntity->has_ownership()){
		return;
	}

	//Set the networked global transform
	global_transform = m_target->get_transform();
}

bool Transform2DSync::get_interpolate() const {
	return m_interpolate;
}

Node2D* Transform2DSync::get_target() {
	return m_target;
}

//This method should only ever be called from the main thread
Vector2 Transform2DSync::get_position() const {
	return global_transform.get_origin();
}

SyncAuthority Transform2DSync::get_authority() const{
	return m_authority;
}

void Transform2DSync::set_interpolate(const bool &interpolate) {
	m_interpolate = interpolate;
}

void Transform2DSync::set_target(Node2D* target) {
	m_target = target;
	//Get the objects transform
	global_transform = m_target->get_transform();
}

//This method should only ever be called from the main thread
void Transform2DSync::set_position(const Vector2 &position) {
	if(!m_target){
		return;
	}

	//Make sure caller has ownership of this entity
	if(!m_parentNetworkEntity->has_ownership()){
		return;
	}

	//Set the networked global transform
	global_transform.set_origin(position);

	//Apply the transform
	m_target->set_transform(global_transform);
}

void Transform2DSync::set_authority(SyncAuthority authority) {
	m_authority = authority;
}




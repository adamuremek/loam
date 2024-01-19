#include "gdnet.h"

Transform3DSync::Transform3DSync() {
	m_target = nullptr;
	m_interpolationTime = 0.1f;
	m_elapsedTime = 0.0f;
	m_authority = SyncAuthority::NONE;
}

void Transform3DSync::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_target", "target"), &Transform3DSync::set_target);
	ClassDB::bind_method(D_METHOD("set_position", "position"), &Transform3DSync::set_position);
	ClassDB::bind_method(D_METHOD("set_authority", "authority"), &Transform3DSync::set_authority);
	ClassDB::bind_method(D_METHOD("get_target"), &Transform3DSync::get_target);
	ClassDB::bind_method(D_METHOD("get_position"), &Transform3DSync::get_position);
	ClassDB::bind_method(D_METHOD("get_authority"), &Transform3DSync::get_authority);
	ClassDB::bind_method(D_METHOD("update_transform_data"), &Transform3DSync::update_transform_data);

	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(OWNER_AUTHORITATIVE);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "target", PROPERTY_HINT_RESOURCE_TYPE, "Node3D"), "set_target", "get_target");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "position", PROPERTY_HINT_RANGE, "-99999,99999,0.001,or_greater,or_less,hide_slider,suffix:m", PROPERTY_USAGE_EDITOR), "set_position", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "authority", PROPERTY_HINT_ENUM, "NONE, OWNER_AUTHORITATIVE", PROPERTY_USAGE_DEFAULT), "set_authority", "get_authority");
}

void Transform3DSync::serialize_payload(EntityUpdateInfo_t &updateInfo) {
	//Resize the data buffer to accomodate the transform information
	updateInfo.dataBuffer.resize(METADATA_SIZE + (12 * sizeof(real_t )));

	//Serialize the target transform into the message data
	serialize_transform3d(global_transform, METADATA_SIZE, updateInfo.dataBuffer);
}

void Transform3DSync::deserialize_payload(const EntityUpdateInfo_t &updateInfo) {
	//Get the target transform from the payload
	global_transform = deserialize_transform3d(METADATA_SIZE, updateInfo.dataBuffer.ptr());
}


void Transform3DSync::transmit_data(HSteamNetConnection destination) {
	if(!m_target){
		return;
	}

	//Create and populate the update info
	EntityUpdateInfo_t updateInfo;

	updateInfo.parentZone = m_parentNetworkEntity->m_info->m_entityInfo.parentZone;
	updateInfo.networkId = m_parentNetworkEntity->m_info->m_entityInfo.networkId;
	updateInfo.updateType = TRANSFORM3D_SYNC_UPDATE;

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

void Transform3DSync::recieve_data(EntityUpdateInfo_t updateInfo) {
	//Obtain and store the transform information within the class
	deserialize_payload(updateInfo);

	if(GDNet::singleton->m_isServer){
		//Reset initial position
		m_parentNetworkEntity->m_info->set_initial_position_3D(global_transform.get_origin());
		//Set the transform
		m_target->call_deferred("set_transform", global_transform);
	}else if(GDNet::singleton->m_isClient){
		call_deferred("update_transform_data");
	}
}

//Must be called from main thread only
void Transform3DSync::interpolate_origin(float delta) {
	m_elapsedTime += delta;
	float t = m_elapsedTime / m_interpolationTime;
	Vector3 interpolatedPosition = m_currentPosition.lerp(m_targetPosition, t);
	m_target->set_position(interpolatedPosition);
}

bool Transform3DSync::has_authority() {
	switch(m_authority){
		case SyncAuthority::NONE:{
			return false;
		}
		case SyncAuthority::OWNER_AUTHORITATIVE:{
			return m_parentNetworkEntity->has_ownership();
		}
		default:
			return false;
	}
}

bool Transform3DSync::has_target() {
	return m_target;
}

void Transform3DSync::update_transform_data() {
	m_currentPosition = m_target->get_position();
	m_targetPosition = global_transform.get_origin();
	m_elapsedTime = 0.0f;
}


Node3D* Transform3DSync::get_target() {
	return m_target;
}

//This method should only ever be called from the main thread
Vector3 Transform3DSync::get_position() const {
	return global_transform.get_origin();
}

SyncAuthority Transform3DSync::get_authority() const{
	return m_authority;
}


void Transform3DSync::set_target(Node3D* target) {
	m_target = target;
	//Get the objects transform
	global_transform = m_target->get_transform();
}

//This method should only ever be called from the main thread
void Transform3DSync::set_position(const Vector3 &position) {
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

void Transform3DSync::set_authority(SyncAuthority authority) {
	m_authority = authority;
}



#include "gdnet.h"

NetworkEntity::NetworkEntity() {
	m_parentZone = nullptr;
}

void NetworkEntity::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_transform3d_sync"), &NetworkEntity::get_transform3d_sync);
	ClassDB::bind_method(D_METHOD("get_transform2d_sync"), &NetworkEntity::get_transform2d_sync);
	ClassDB::bind_method(D_METHOD("get_entity_info"), &NetworkEntity::get_entity_info);

	ClassDB::bind_method(D_METHOD("set_transform3d_sync", "transform3d_sync"), &NetworkEntity::set_transform3d_sync);
	ClassDB::bind_method(D_METHOD("set_transform2d_sync", "transform2d_sync"), &NetworkEntity::set_transform2d_sync);

	ClassDB::bind_method(D_METHOD("client_side_transmit_data"), &NetworkEntity::CLIENT_SIDE_transmit_data);
	ClassDB::bind_method(D_METHOD("server_side_transmit_data"), &NetworkEntity::SERVER_SIDE_transmit_data);
}

void NetworkEntity::_notification(int n_type) {
	//Break out of the notification if this code is being run from the editor
	if(Engine::get_singleton()->is_editor_hint()){
		return;
	}

	switch (n_type) {
		case NOTIFICATION_READY: {
			_ready();
			break;
		}
		case NOTIFICATION_ENTER_TREE: {
			break;
		}
		case NOTIFICATION_EXIT_TREE: {
			break;
		}
		//TODO: This peice of shit requires the "func _process()" method to exist in gdscript to work. no idea why
		case NOTIFICATION_PROCESS:{
			_process(get_process_delta_time());
			break;
		}
	}
}

void NetworkEntity::_ready() {
	if(GDNet::singleton->is_client()){
		if(m_transform3DSync.is_valid() && m_transform3DSync->has_target()){
			//Set the starting transform so it doesnt start interpolating from the origin
			m_transform3DSync->update_transform_data();
		}

		if(m_transform2DSync.is_valid() && m_transform2DSync->has_target()){
			//Set the starting transform so it doesnt start interpolating from the origin
			m_transform2DSync->update_transform_data();
		}

	}
}

void NetworkEntity::_process(float delta) {
	if(GDNet::singleton->is_client()){
		if(m_transform3DSync.is_valid() && m_transform3DSync->has_target()){
			if(!has_ownership() && m_transform3DSync->get_authority() == SyncAuthority::OWNER_AUTHORITATIVE){
				m_transform3DSync->interpolate_origin(delta);
			}
		}

		if(m_transform2DSync.is_valid() && m_transform2DSync->has_target()){
			if(!has_ownership() && !m_transform2DSync->has_authority()){
				m_transform2DSync->interpolate_origin(delta);
			}
		}
	}
}


bool NetworkEntity::has_ownership() {
	if(GDNet::singleton->m_isClient){
		return m_info->get_owner_id() == GDNet::singleton->world->get_player_id();
	}
	else if(GDNet::singleton->m_isServer){
		return m_info->get_owner_id() == 0;
	}

	return false;
}

void NetworkEntity::SERVER_SIDE_recieve_data(EntityUpdateInfo_t updateInfo) {
	switch (updateInfo.updateType) {
		case TRANSFORM3D_SYNC_UPDATE:{
			//Make sure a Transform3DSync module instance exists in case :p
			if(m_transform3DSync.is_valid()){
				m_transform3DSync->recieve_data(updateInfo);
			}
			break;
		}
		case TRANSFORM2D_SYNC_UPDATE:{
			if(m_transform2DSync.is_valid()){
				m_transform2DSync->recieve_data(updateInfo);
			}
		}
	}
}

void NetworkEntity::SERVER_SIDE_transmit_data() {
	//If a transform3dsync object was assigned, use it.
//	if(m_transform3DSync.is_valid()){
//		//Send the data to all players in the zone
//		for(const KeyValue<PlayerID_t, Ref<PlayerInfo>> &player : m_parentZone->m_playersInZone){
//			m_transform3DSync->transmit_data(player.value->get_player_conn());
//		}
//	}

	if(m_transform2DSync.is_valid()){
		m_transform2DSync->tick();
	}
}

void NetworkEntity::CLIENT_SIDE_recieve_data(EntityUpdateInfo_t updateInfo) {
	switch (updateInfo.updateType) {
		case TRANSFORM3D_SYNC_UPDATE:{
			//Make sure a Transform3DSync module instance exists in case :p
			if(m_transform3DSync.is_valid()){
				//Only read from the data if the endpoint does not have authority
				if(!m_transform3DSync->has_authority()){
					m_transform3DSync->recieve_data(updateInfo);
				}
			}
			break;
		}
		case TRANSFORM2D_SYNC_UPDATE:{

			//Make sure a Transform2DSync module instance exists in case :p
			if(m_transform2DSync.is_valid()){
				//Only read from the data if the endpoint does not have authority
				if(!m_transform2DSync->has_authority()){
					m_transform2DSync->recieve_data(updateInfo);
				}
			}
			break;
		}
	}
}

void NetworkEntity::CLIENT_SIDE_transmit_data() {
	//If a transform3dsync object was assigned, use it.
//	if(m_transform3DSync.is_valid() && m_transform3DSync->has_authority()){
//		m_transform3DSync->transmit_data(GDNet::singleton->world->m_worldConnection);
//	}

	if(m_transform2DSync.is_valid()){
		m_transform2DSync->tick();
	}
}


Ref<Transform3DSync> NetworkEntity::get_transform3d_sync() {
	return m_transform3DSync;
}

Ref<Transform2DSync> NetworkEntity::get_transform2d_sync() {
	return m_transform2DSync;
}

Ref<EntityInfo> NetworkEntity::get_entity_info() {
	return m_info;
}


void NetworkEntity::set_transform3d_sync(Ref<Transform3DSync> transform3DSync) {
	m_transform3DSync = transform3DSync;
	m_transform3DSync->m_parentNetworkEntity = this;
}

void NetworkEntity::set_transform2d_sync(Ref<Transform2DSync> transform2DSync) {
	m_transform2DSync = transform2DSync;
	m_transform2DSync->m_parentNetworkEntity = this;
}





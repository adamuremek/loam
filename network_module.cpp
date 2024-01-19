#include "gdnet.h"

const int NetworkModule::METADATA_SIZE = 1 + sizeof(ZoneID_t) + sizeof(EntityNetworkID_t) + 1;

void NetworkModule::serialize_payload(EntityUpdateInfo_t &updateInfo) {}
void NetworkModule::deserialize_payload(const EntityUpdateInfo_t &updateInfo) {}
void NetworkModule::transmit_data(HSteamNetConnection destination) {}
void NetworkModule::recieve_data(EntityUpdateInfo_t updateInfo) {}

void NetworkModule::serialize_update_metadata(EntityUpdateInfo_t &updateInfo) {
	updateInfo.dataBuffer.resize(METADATA_SIZE);
	int bufferIdx = 0;

	//Set the message type of the message data
	updateInfo.dataBuffer.set(bufferIdx, NETWORK_ENTITY_UPDATE);
	bufferIdx += 1;

	//Serialize the parent zone ID into the message data
	serialize_uint(updateInfo.parentZone, bufferIdx, updateInfo.dataBuffer);
	bufferIdx += sizeof(ZoneID_t );

	//Serialize the network ID into the message data
	serialize_uint(updateInfo.networkId, bufferIdx, updateInfo.dataBuffer);
	bufferIdx += sizeof(EntityNetworkID_t);

	//Add the update type tot he message data
	updateInfo.dataBuffer.set(bufferIdx, updateInfo.updateType);
}

EntityUpdateInfo_t NetworkModule::deserialize_update_metadata(const unsigned char *mssgData, const int mssgLen) {
	//Create a new entity update object
	EntityUpdateInfo_t updateInfo;

	//Load message data into the update info data buffer
	updateInfo.dataBuffer.resize(mssgLen);
	for(int i = 0; i < mssgLen; ++i){
		updateInfo.dataBuffer.set(i, mssgData[i]);
	}

	//Use a data index counter in case data type sizes differ between architecture
	int dataIdx = 1;

	//Deserialize the parent zone ID
	updateInfo.parentZone = deserialize_uint(dataIdx, mssgData);
	dataIdx += sizeof(ZoneID_t);

	//Deserialize the entity network ID
	updateInfo.networkId = deserialize_uint(dataIdx, mssgData);
	dataIdx += sizeof(EntityNetworkID_t);

	//Deserialize (not really lol) the update type
	updateInfo.updateType = mssgData[dataIdx];

	return updateInfo;
}
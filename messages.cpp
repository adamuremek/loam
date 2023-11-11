#include "gdnet.h"
#include "include/steam/isteamnetworkingutils.h"
#include "include/steam/steamnetworkingsockets.h"

void serialize_uint(unsigned int value, int startIdx, unsigned char* buffer){
	for(int i = startIdx; i < startIdx + 4; i++){
		buffer[i] = static_cast<unsigned char>(value & 0xFF);
		value >>= 8;
	}
}

void copy_string_to_buffer(const char* string, unsigned char * buffer, int startIDx, int stringSize){
	for(int i = 0; i < stringSize; i++){
		buffer[startIDx] = string[i];
		startIDx++;
	}
}

SteamNetworkingMessage_t *allocate_message(const unsigned char* data, const int sizeOfData, const HSteamNetConnection &destination){
	// Allocate a new message
	SteamNetworkingMessage_t *pMessage = SteamNetworkingUtils()->AllocateMessage(sizeOfData);

	//Sanity check: make sure message was created
	if (!pMessage) {
		ERR_PRINT("Unable to create message!");
		return nullptr;
	}

	//Copy message data into the message's data buffer
	memcpy(pMessage->m_pData, data, sizeOfData);
	pMessage->m_cbSize = sizeOfData;

	//Set the message target connection
	pMessage->m_conn = destination;

	return pMessage;
}

SteamNetworkingMessage_t *create_mini_message(MessageType_t messageType, unsigned int value, const HSteamNetConnection &destination) {
	//Create the message data
	const int sizeOfData = 1 + sizeof(unsigned int);
	unsigned char data[sizeOfData];

	//Assign message type in the buffer
	data[0] = messageType;

	//Populate message data
	serialize_uint(value, 1, data);

	// Return message
	return allocate_message(data, sizeOfData, destination);
}

SteamNetworkingMessage_t *create_small_message(MessageType_t messageType, unsigned int value1, unsigned int value2, const HSteamNetConnection &destination) {
	// Create the message data
	const int sizeOfData = 1 + (2 * sizeof(unsigned int));
	unsigned char data[sizeOfData];

	//Assign message type in the buffer
	data[0] = messageType;

	// Populate the message data
	//TODO: Maybe reconsider hardcoding the start index, differnt architectures could define the size of the types diffently and byte ;) me in the ass.
	serialize_uint(value1, 1, data);
	serialize_uint(value2, 5, data);

	// Return the message
	return allocate_message(data, sizeOfData, destination);
}

SteamNetworkingMessage_t *instantiate_entity_message(const EntityID_t entityID, String parentNode, const HSteamNetConnection &destination){
	// Create the message data
	const int sizeOfData = 1 + sizeof(EntityID_t) + parentNode.length();
	unsigned char data[sizeOfData];

	//Assign message type in the buffer
	data[0] = INSTANTIATE_NETWORK_ENTITY;

	// Populate message data
	serialize_uint(entityID, 1, data);
	copy_string_to_buffer(parentNode.utf8().get_data(), data, 5, parentNode.length());

	//Return the message
	allocate_message(data, sizeOfData, destination);
}

unsigned int deserialize_mini(const char *data) {
	unsigned int result = 0;

	for (int i = 1; i <= sizeof(unsigned int); i++) {
		result |= (static_cast<unsigned int>(static_cast<unsigned char>(data[i]) << (8 * i)));
	}

	return result;
}

void deserialize_small(const char *data, unsigned int &value1, unsigned int &value2) {
	unsigned int result1 = 0, result2 = 0;

	int i = 1;
	for (; i <= sizeof(unsigned int); i++) {
		result1 |= (static_cast<unsigned int>(static_cast<unsigned char>(data[i]) << (8 * i)));
	}

	for (; i <= 2 * sizeof(unsigned int); i++) {
		result2 |= (static_cast<unsigned int>(static_cast<unsigned char>(data[i]) << (8 * i)));
	}

	value1 = result1;
	value2 = result2;
}

void send_message(const HSteamNetConnection &destination, SteamNetworkingMessage_t *message) {
	//Send the message
	SteamNetworkingSockets()->SendMessageToConnection(destination, message->m_pData, message->m_cbSize, k_nSteamNetworkingSend_Reliable, nullptr);
	//Free memory from the message
	message->Release();
}

#include "gdnet.h"
#include "include/steam/steamnetworkingsockets.h"
#include "include/steam/isteamnetworkingutils.h"



SteamNetworkingMessage_t* create_mini_message(const int messageType, unsigned int value, const HSteamNetConnection& destination) {
	//Create the message data
	const int sizeOfData = 1 + sizeof(unsigned int);
	char data[sizeOfData];

	//Populate message data
	for (int i = 1; i <= sizeof(unsigned int); i++) {
		data[i] = static_cast<char>(value & 0xFF);
		value >>= 8;
	}

	//Allocate a new message
	SteamNetworkingMessage_t *pMessage = SteamNetworkingUtils()->AllocateMessage(sizeOfData);

	//Sanity check: make sure message was created
	if (!pMessage) {
		ERR_FAIL_MSG("Unable to create message!");
		return nullptr;
	}

	//Copy message data into the message's data buffer
	memcpy(pMessage->m_pData, data, sizeOfData);
	pMessage->m_cbSize = sizeOfData;

	//Set the message target connection
	pMessage->m_conn = destination;

	return pMessage;
}

SteamNetworkingMessage_t* create_small_message(const int messageType, unsigned int value1, unsigned int value2, const HSteamNetConnection& destination) {
	// Create the message data
	const int sizeOfData = 1 + (2 * sizeof(unsigned int));
	char data[sizeOfData];

	// Populate the message data
	int i = 1;
	for (i; i <= sizeof(unsigned int); i++) {
		data[i] = static_cast<char>(value1 & 0xFF);
		value1 >>= 8;
	}

	for (i; i <= 2 * sizeof(unsigned int); i++) {
		data[i] = static_cast<char>(value2 & 0xFF);
		value2 >>= 8;
	}

	// Allocate a new message
	SteamNetworkingMessage_t *pMessage = SteamNetworkingUtils()->AllocateMessage(sizeOfData);

	//Sanity check: make sure message was created
	if (!pMessage) {
		ERR_FAIL_MSG("Unable to create message!");
		return nullptr;
	}

	//Copy message data into the message's data buffer
	memcpy(pMessage->m_pData, data, sizeOfData);
	pMessage->m_cbSize = sizeOfData;

	//Set the message target connection
	pMessage->m_conn = destination;

	return pMessage;
}

unsigned int deserialize_mini(const char *data) {
	unsigned int result = 0;

	for (int i = 1; i <= sizeof(unsigned int); i++) {
		result |= (static_cast<unsigned int>(static_cast<unsigned char>(data[i]) << (8 * i)));
	}

	return result;
}

void deserialize_small(const char* data, unsigned int& value1, unsigned int& value2) {
	unsigned int result1 = 0, result2 = 0;

	int i = 1;
	for (i; i <= sizeof(unsigned int); i++) {
		result1 |= (static_cast<unsigned int>(static_cast<unsigned char>(data[i]) << (8 * i)));
	}

	for (i; i <= 2 * sizeof(unsigned int); i++) {
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





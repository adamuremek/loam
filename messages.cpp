#include "gdnet.h"


void serialize_int(int value, int startIdx, Vector<unsigned char> &buffer){
	for (int i = startIdx; i < startIdx + sizeof(int); i++) {
		buffer.set(i, static_cast<unsigned char>(value & 0xFF));
		value >>= 8;
	}
}

void serialize_uint(unsigned int value, int startIdx, Vector<unsigned char> &buffer) {
	for (int i = startIdx; i < startIdx + sizeof(unsigned int); i++) {
		buffer.set(i, static_cast<unsigned char>(value & 0xFF));
		value >>= 8;
	}
}

int deserialize_int(int startIdx, const unsigned char* buffer){
	int res = 0;
	int shiftAmt = 0;

	for(int i = startIdx; i < startIdx + sizeof(unsigned int); i++){
		res |= buffer[i] << (8 * shiftAmt);
		shiftAmt++;
	}

	return res;
}

unsigned int deserialize_uint(int startIdx, const unsigned char* buffer){
	unsigned int res = 0U;
	int shiftAmt = 0;

	for(int i = startIdx; i < startIdx + sizeof(unsigned int); i++){
		res |= buffer[i] << (8 * shiftAmt);
		shiftAmt++;
	}

	return res;
}

String deserialize_string(int startIdx, int stringLength, const unsigned char* buffer){
	Vector<unsigned char> strData;
	strData.resize(stringLength);

	for(int i = 0; i < stringLength; i++){
		strData.set(i, buffer[startIdx + i]);
	}

	return String((const char*)strData.ptr());
}

void copy_string_to_buffer(const char *string, unsigned char *buffer, int startIDx, int stringSize) {
	for (int i = 0; i < stringSize; i++) {
		buffer[startIDx] = string[i];
		startIDx++;
	}
}

SteamNetworkingMessage_t *allocate_message(const unsigned char *data, const int sizeOfData, const HSteamNetConnection &destination) {
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
	Vector<unsigned char> data;
	data.resize(sizeOfData);

	//Assign message type in the buffer
	data.set(0, messageType);

	//Populate message data
	serialize_uint(value, 1, data);

	unsigned char* test = data.ptrw();

	// Return message
	return allocate_message(data.ptr(), sizeOfData, destination);
}

SteamNetworkingMessage_t *create_small_message(MessageType_t messageType, unsigned int value1, unsigned int value2, const HSteamNetConnection &destination) {
	// Create the message data
	const int sizeOfData = 1 + (2 * sizeof(unsigned int));
	Vector<unsigned char> data;
	data.resize(sizeOfData);

	//Assign message type in the buffer
	data.set(0, messageType);

	// Populate the message data
	//TODO: Maybe reconsider hardcoding the start index, differnt architectures could define the size of the types diffently and byte ;) me in the ass.
	serialize_uint(value1, 1, data);
	serialize_uint(value2, 5, data);

	// Return the message
	return allocate_message(data.ptr(), sizeOfData, destination);
}



unsigned int deserialize_mini(const unsigned char *data) {
	return deserialize_uint(1, data);
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

void send_message_reliable(SteamNetworkingMessage_t *message) {
	//Send the message
	SteamNetworkingSockets()->SendMessageToConnection(message->m_conn, message->m_pData, message->m_cbSize, k_nSteamNetworkingSend_Reliable, nullptr);
	//Free memory from the message
	message->Release();
}

void send_message_unreliable(SteamNetworkingMessage_t *message){
	//Send the message
	SteamNetworkingSockets()->SendMessageToConnection(message->m_conn, message->m_pData, message->m_cbSize, k_nSteamNetworkingSend_Unreliable, nullptr);
	//Free memory from the message
	message->Release();
}

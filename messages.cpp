#include "gdnet.h"

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
	//TODO: Maybe reconsider hardcoding the start index, different architectures could define the size of the types diffently and byte ;) me in the ass.
	serialize_uint(value1, 1, data);
	serialize_uint(value2, 5, data);

	// Return the message
	return allocate_message(data.ptr(), sizeOfData, destination);
}


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

void serialize_vector3(const Vector3 &vec, int startIdx, Vector<unsigned char> &buffer){
	memcpy(buffer.ptrw() + startIdx, &vec, sizeof(Vector3));
}

void serialize_vector2(const Vector2 &vec, int startIdx, Vector<unsigned char> &buffer){
	memcpy(buffer.ptrw() + startIdx, &vec, sizeof(Vector2));
}

void serialize_transform3d(const Transform3D &transform, int startIdx, Vector<unsigned char> &buffer){
	Vector3 origin = transform.get_origin();
	Basis basis = transform.get_basis();

	//Serialize the origin
	memcpy(buffer.ptrw() + startIdx, &origin, sizeof(Vector3));
	startIdx += sizeof(Vector3);

	//Serialize the basis
	memcpy(buffer.ptrw() + startIdx, &basis, sizeof(Basis));
}

void serialize_transform2d(const Transform2D &transform, int startIdx, Vector<unsigned char> &buffer){
	memcpy(buffer.ptrw() + startIdx, &transform, sizeof(Transform2D));
}

//template<typename T>
//void serialize_basic(const T &value, int startIdx, Vector<unsigned char> &buffer){
//	memcpy(buffer.ptrw() + startIdx, &value, sizeof(T));
//}
//
//template<typename T>
//T deserialize_basic(int startIdx, const unsigned char *buffer){
//	T incomingVal;
//
//	//Copy data from buffer into the type
//	memcpy(*incomingVal, buffer + startIdx, sizeof(T));
//
//	return incomingVal;
//}

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

Vector3 deserialize_vector3(int startIdx, const unsigned char *buffer){
	Vector3 incomingVec;

	//Copy data from buffer to the struct
	memcpy(&incomingVec, buffer + startIdx, sizeof(Vector3));

	return incomingVec;
}

Transform3D deserialize_transform3d(int startIdx, const unsigned char *buffer){
	real_t incomingData[12]; //Array to stroe the incoming data

	//Copy data directly from buffer to incomingData
	memcpy(incomingData, buffer + startIdx, sizeof(incomingData));

	//Construct origin and basis from incomingData
	Vector3 origin(incomingData[0], incomingData[1], incomingData[2]);
	Basis basis(incomingData[3],incomingData[4],incomingData[5],
				incomingData[6],incomingData[7],incomingData[8],
				incomingData[9],incomingData[10],incomingData[11]);

	//Create and return the Transform3D object
	return Transform3D(basis, origin);
}

unsigned int deserialize_mini(const unsigned char *data) {
	return deserialize_uint(1, data);
}

void deserialize_small(const unsigned char *data, unsigned int &value1, unsigned int &value2) {
	value1 = deserialize_uint(1, data);
	value2 = deserialize_uint(5, data);
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

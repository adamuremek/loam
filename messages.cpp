#include "gdnet.h"
#include "include/steam/steamnetworkingsockets.h"
#include "include/steam/isteamnetworkingutils.h"


PlayerID_t generate_player_id() {
	PlayerID_t id = id_counter;
	id_counter++;

	return id;
}

SteamNetworkingMessage_t* create_player_id_assignment_message(const PlayerConnectionInfo& player) {
	//Create the message data
	char data[3];
	data[0] = ASSIGN_PLAYER_ID;
	data[1] = player.id & 0XFF;
	data[2] = (player.id >> 8) & 0xFF;

	//Allocate a new message
	SteamNetworkingMessage_t *pMessage = SteamNetworkingUtils()->AllocateMessage(3);

	//Sanity check: make sure message was created
	if (!pMessage) {
		print_line("Unable to create message!");
		return nullptr;
	}

	//Copy message data into the message's data buffer
	memcpy(pMessage->m_pData, data, 3);
	pMessage->m_cbSize = 3;

	//Set the message target connection
	pMessage->m_conn = player.m_hConn;

	return pMessage;
}

void send_message(const PlayerConnectionInfo &player, SteamNetworkingMessage_t *message) {
	//Send the message
	SteamNetworkingSockets()->SendMessageToConnection(player.m_hConn, message->m_pData, message->m_cbSize, k_nSteamNetworkingSend_Reliable, nullptr);
	//Free memory from the message
	message->Release();
}

PlayerID_t deserialize_id(const char* data, int startIdx) {
	return (unsigned short)(data[startIdx]) | ((unsigned short)(data[startIdx + 1]) << 8);
}

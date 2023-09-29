#include "gdnet.h"
#include "core/os/memory.h"
#include "core/string/print_string.h"
#include "core/error/error_macros.h"
#include "core/core_string_names.h"
#include "include/steam/steamnetworkingsockets.h"
#include "include/steam/isteamnetworkingutils.h"
#include <thread>


//===============World Implementation===============//

World *World::s_pCallbackInstace = nullptr;

World::World() {
	s_pCallbackInstace = this;
	m_pInterface = SteamNetworkingSockets();
	m_hListenSock = k_HSteamListenSocket_Invalid;
	m_hPollGroup = k_HSteamNetPollGroup_Invalid;
}

World::~World() {
	
}

//==Private Methods==//

void World::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start_server", "port"), &World::start_world);
}

void World::steam_net_conn_status_changed_wrapper(SteamNetConnectionStatusChangedCallback_t* pInfo) {
	s_pCallbackInstace->on_net_connection_status_changed(pInfo);
}

void World::on_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t* pInfo) {
	//What is the state of the connection?
	switch (pInfo->m_info.m_eState) {

		case k_ESteamNetworkingConnectionState_None:
			//NOTE: Callbacks will be sent here when connections are destoryed. These can be ignored
			break;

		case k_ESteamNetworkingConnectionState_ClosedByPeer:
			remove_player(pInfo->m_hConn);
			break;

		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
			break;

		case k_ESteamNetworkingConnectionState_Connecting:
			//Make sure the connecting player isnt already connected
			if (m_worldPlayerConnections.find(pInfo->m_hConn) == m_worldPlayerConnections.end())
				break;

			print_line("Connection request recieved.");

			//A client is attempting to connect
			//Try to accept the connection
			if (m_pInterface->AcceptConnection(pInfo->m_hConn) != k_EResultOK) {
				// This could fail.  If the remote host tried to connect, but then
				// disconnected, the connection may already be half closed.  Just
				// destroy whatever we have on our side.
				m_pInterface->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
				print_line("Can't accept connection.  (It was already closed?)");
				break;
			}

			// Assign the poll group
			if (!m_pInterface->SetConnectionPollGroup(pInfo->m_hConn, m_hPollGroup)) {
				m_pInterface->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
				print_line("Failed to set poll group?");
				break;
			}

			break;

		case k_ESteamNetworkingConnectionState_Connected:
			// We will get a callback immediately after accepting the connection.

			//Make sure the connecting player isnt already connected (i have no idea if/why it would here, but just in case :D )
			if (m_worldPlayerConnections.find(pInfo->m_hConn) == m_worldPlayerConnections.end())
				break;

			//Populate player info
			PlayerConnectionInfo player;
			player.m_hConn = pInfo->m_hConn;

			//Add player to list of active player connections
			m_worldPlayerConnections.insert({ pInfo->m_hConn, player });

			break;

		default:
			//No mans land
			//God knows what the hell would end up here. Hopefully nothing important
			break;
	}
}

void World::recieve_run_loop() {
	while (m_serverLoop) {
		poll_incoming_messages();
		m_pInterface->RunCallbacks();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void World::poll_incoming_messages() {
	while (m_serverLoop) {
		ISteamNetworkingMessage *pIncomingMessage = nullptr;
		int numMsgs = m_pInterface->ReceiveMessagesOnPollGroup(m_hPollGroup, &pIncomingMessage, 1);

		if (numMsgs == 0)
			break;

		if (numMsgs < 0) {
			ERR_FAIL_MSG("Error checking messages");
			m_serverLoop = false;
			return;
		}
	}
}

void World::remove_player(HSteamNetConnection hConn) {
	//Remove the player info from the map if they exist
	if (m_worldPlayerConnections.find(hConn) != m_worldPlayerConnections.end())
		m_worldPlayerConnections.erase(hConn);

	//Close connection witht the player
	if (!m_pInterface->CloseConnection(hConn, 0, nullptr, false))
		print_line("Cannot close connection, it was already closed.");
}

//==Public Methods==//

void World::start_world(int port) {
	//Define network interface to listen on
	SteamNetworkingIPAddr serverAddress;
	serverAddress.Clear();
	serverAddress.m_port = port;

	//Server Config (set callbacks)
	SteamNetworkingConfigValue_t cfg;
	cfg.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)World::steam_net_conn_status_changed_wrapper);

	//Create a listening socket with the server config
	m_hListenSock = m_pInterface->CreateListenSocketIP(serverAddress, 1, &cfg);

	//Make sure the listening socket is valid, otherwise report an error and terminate start procedure.
	if (m_hListenSock == k_HSteamListenSocket_Invalid)
	{
		ERR_FAIL_MSG(vformat("Failed to listen on port %d (invalid socket)", port));
		return;
	}

	//Make sure the poll group is valid, otherwirse report and error and terminate the start prcedure.
	if (m_hPollGroup == k_HSteamNetPollGroup_Invalid) {
		ERR_FAIL_MSG(vformat("Failed to listen on port %d (invalid poll group)", port));
	}

	//Start the main server loop
	m_serverLoop = true;
	m_recieveLoopThread = std::thread(&World::recieve_run_loop, this);

	//TEMP: confirm that the server has started on the requested port:
	print_line(vformat("Server has started on port %d!", port));
}

void World::stop_world() {
	m_serverLoop = false;

	if (m_recieveLoopThread.joinable())
		m_recieveLoopThread.join();

	//Close the socket
	m_pInterface->CloseListenSocket(m_hListenSock);
	m_hListenSock = k_HSteamListenSocket_Invalid;

	//Destroy poll group
	m_pInterface->DestroyPollGroup(m_hPollGroup);
	m_hPollGroup = k_HSteamNetPollGroup_Invalid;
}



//===============GDNetClient Implementation===============//

GDNetClient::GDNetClient() {
}

GDNetClient::~GDNetClient() {

}

void GDNetClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("connect_to_server", "host", "port"), &GDNetClient::connect_to_server);
}

void GDNetClient::connect_to_server(String host, int port) {

}


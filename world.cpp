#include "gdnet.h"
#include "core/os/memory.h"
#include "core/string/print_string.h"
#include "core/error/error_macros.h"
#include "core/core_string_names.h"
#include "include/steam/isteamnetworkingsockets.h"
#include "include/steam/isteamnetworkingutils.h"
#include <thread>


//===============World Implementation===============//

World *World::s_pCallbackInstace = nullptr;

World::World() {
	s_pCallbackInstace = this;
	m_hListenSock = k_HSteamListenSocket_Invalid;
	m_hPollGroup = k_HSteamNetPollGroup_Invalid;
	m_runLoop = false;
	GDNet::get_singleton()->m_player.m_world = this;
}

World::~World() {
	s_pCallbackInstace = nullptr;
	//When called here, this mehtod does not allow the editor to start for some reason.
	//stop_world();
}

//==Private Methods==//

//=======================================GAMENETWORKINGSOCKETS STUFF=======================================//
void World::steam_net_conn_status_changed_wrapper(SteamNetConnectionStatusChangedCallback_t* pInfo) {
	s_pCallbackInstace->on_net_connection_status_changed(pInfo);
}

void World::player_connecting(HSteamNetConnection playerConnection) {
	print_line("Player is connecting...");

	// Make sure the connecting player isnt already connected (or isnt already int he playerconnections map)
	if (m_worldPlayerInfoByConnection.find(playerConnection) != m_worldPlayerInfoByConnection.end()) {
		return;
	}

	// A client is attempting to connect
	// Try to accept the connection
	if (SteamNetworkingSockets()->AcceptConnection(playerConnection) != k_EResultOK) {
		// This could fail.  If the remote host tried to connect, but then
		// disconnected, the connection may already be half closed.  Just
		// destroy whatever we have on our side.
		SteamNetworkingSockets()->CloseConnection(playerConnection, 0, nullptr, false);
		print_line("Can't accept connection.  (It was already closed?)");
		return;
	}

	// Assign the poll group
	if (!SteamNetworkingSockets()->SetConnectionPollGroup(playerConnection, m_hPollGroup)) {
		SteamNetworkingSockets()->CloseConnection(playerConnection, 0, nullptr, false);
		print_line("Failed to set poll group?");
		return;
	}
}

void World::player_connected(HSteamNetConnection playerConnection) {
	//Make sure the connecting player isnt already connected (i have no idea if/why it would here, but just in case :D )
	if (m_worldPlayerInfoByConnection.find(playerConnection) != m_worldPlayerInfoByConnection.end()) {
		return;
	}

	//Populate player info
	PlayerConnectionInfo player;
	player.m_hConn = playerConnection;
	player.id = IDGenerator::generatePlayerID();

	//Send the player their ID
	SteamNetworkingMessage_t *idAssignmentMssg = create_mini_message(ASSIGN_PLAYER_ID, player.id, player.m_hConn);
	send_message(player.m_hConn, idAssignmentMssg);

	//Add player to a map keyed by connection, and a map keyed by id
	m_worldPlayerInfoByConnection.insert({ playerConnection, player });
	m_worldPlayerInfoById.insert({ player.id, player });

	print_line("Player has connected!");
}

void World::player_disconnected(HSteamNetConnection playerConnection) {
	print_line("Player has disconnected!");
	remove_player(playerConnection);
}

void World::on_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t* pInfo) {
	//What is the state of the connection?
	switch (pInfo->m_info.m_eState) {

		case k_ESteamNetworkingConnectionState_None:
			//NOTE: Callbacks will be sent here when connections are destoryed. These can be ignored
			break;

		case k_ESteamNetworkingConnectionState_ClosedByPeer: {
			player_disconnected(pInfo->m_hConn);
			break;
		}

		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
			print_line("Player connection has dropped improperly!");
			break;

		case k_ESteamNetworkingConnectionState_Connecting: 
			player_connecting(pInfo->m_hConn);
			break;

		case k_ESteamNetworkingConnectionState_Connected:
			// We will get a callback immediately after accepting the connection.
			player_connected(pInfo->m_hConn);
			break;
			
		default:
			//No mans land
			//God knows what the hell would end up here. Hopefully nothing important :3
			break;
	}
}

//=========================================================================================================//

void World::run_loop() {
	while (m_runLoop) {
		poll_incoming_messages();
		SteamNetworkingSockets()->RunCallbacks();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void World::poll_incoming_messages() {
	while (m_runLoop) {
		SteamNetworkingMessage_t *pIncomingMsgs[16];
		int numMsgs = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(m_hPollGroup, pIncomingMsgs, 16);

		if (numMsgs == 0)
			break;

		if (numMsgs < 0) {
			ERR_FAIL_MSG("Error checking messages");
			m_runLoop = false;
			return;
		}

		//Evaluate each message
		for (int i = 0; i < numMsgs; i++) {
			SteamNetworkingMessage_t *pMessage = pIncomingMsgs[i];
			const char *mssgData = static_cast<char *>(pMessage->m_pData);

			//Check the type of message recieved
			switch (mssgData[0]) {
				case LOAD_ZONE_REQUEST: {
					print_line("Load Zone Request recieved!");
					// Get the player id and the zone requested by the player
					PlayerID_t playerId = m_worldPlayerInfoByConnection[pMessage->m_conn].id;
					ZoneID_t zoneId = deserialize_mini(mssgData);
					Zone *zone = m_registeredZones[zoneId];

					// Instnatiate the zone if it isnt already or if there are no players in the zone.
					if (!zone->m_instantiated || m_registeredZones.size() == 0) {
						zone->call_deferred("instantiate_zone");
					}

					// Inform all players in the zone that a new player has loaded into the zone
					for (const PlayerID_t &id : zone->m_playersInZone) {
						HSteamNetConnection destination = m_worldPlayerInfoById[id].m_hConn;
						SteamNetworkingMessage_t *pPlayerEnteredZoneMssg = create_mini_message(PLAYER_ENTERED_ZONE, playerId, destination);
						send_message(destination, pPlayerEnteredZoneMssg);
					}

					// Add requesting player to the zone locally
					zone->add_player(playerId);

					SteamNetworkingMessage_t *pOutgoingMssg = create_mini_message(LOAD_ZONE_ACCEPT, zoneId, pMessage->m_conn);
					send_message(pMessage->m_conn, pOutgoingMssg);
					break;
				}
				default:
					break;
			}

			//Dispose of the message
			pMessage->Release();
		}
	}
}

void World::remove_player(HSteamNetConnection hConn) {
	//Remove the player info from the connection keyed map and the id keyed map if they exist
	std::map<HSteamNetConnection, PlayerConnectionInfo>::iterator iter = m_worldPlayerInfoByConnection.find(hConn);
	if (iter != m_worldPlayerInfoByConnection.end()) {
		//Assign found value to a variable
		PlayerConnectionInfo info = iter->second;
		m_worldPlayerInfoById.erase(info.id);
		m_worldPlayerInfoByConnection.erase(hConn);
	}
		

	//Close connection witht the player
	if (!SteamNetworkingSockets()->CloseConnection(hConn, 0, nullptr, false)) {
		print_line("Cannot close connection, it was already closed.");
	}

	print_line("Connection with a player has been closed.");
}

//==Protected Mehtods==//

void World::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start_world", "port"), &World::start_world);
	ClassDB::bind_method(D_METHOD("stop_world"), &World::stop_world);
	ClassDB::bind_method(D_METHOD("join_world", "world", "port"), &World::join_world);
	ClassDB::bind_method(D_METHOD("leave_world"), &World::leave_world);
	ClassDB::bind_method(D_METHOD("load_zone", "zone_name"), &World::load_zone);

	ADD_SIGNAL(MethodInfo("joined_world"));
}

void World::_notification(int n_type) {
	switch (n_type) {
		case NOTIFICATION_EXIT_TREE: {

			//If the application closes, the world node will be cleaned up.
			//When the world node is cleaned up, properly disconnect the player from the world
			//to ensure a smooth cleanup.
			if (GDNet::get_singleton()->m_player.m_isConnectedToWorld) {
				GDNet::get_singleton()->m_player.disconnect_from_world();
			}

			break;
		}
	}
}


//==Public Methods==//

void World::register_zone(Zone *zone) {
	m_registeredZones.insert({ zone->get_zone_id(), zone });
}

void World::unregister_zone(Zone *zone) {
	m_registeredZones.erase(zone->get_zone_id());
}

bool World::instantiate_zone_by_id(ZoneID_t zoneId) {
	std::map<ZoneID_t, Zone *>::iterator iter = m_registeredZones.find(zoneId);

	if (iter != m_registeredZones.end()) {
		iter->second->instantiate_zone();
		return true;
	} else {
		return false;
	}
}

void World::start_world(int port) {
	if (!GDNet::get_singleton()->m_isInitialized) {
		ERR_FAIL_MSG("GDNet is not initialized!");
		return;
	}

	//Define network connection info
	SteamNetworkingIPAddr worldInfo;
	worldInfo.Clear();
	worldInfo.m_port = (uint16)port;

	//Create a poll group
	m_hPollGroup = SteamNetworkingSockets()->CreatePollGroup();

	//Server Config (set callbacks)
	SteamNetworkingConfigValue_t cfg;
	cfg.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)World::steam_net_conn_status_changed_wrapper);
	
	//Create a listening socket with the server config
	m_hListenSock = SteamNetworkingSockets()->CreateListenSocketIP(worldInfo, 1, &cfg);

	//Make sure the listening socket is valid, otherwise report an error and terminate start procedure.
	if (m_hListenSock == k_HSteamListenSocket_Invalid)
	{
		ERR_FAIL_MSG(vformat("Failed to listen on port %d (invalid socket)", port));
		return;
	}

	//Make sure the poll group is valid, otherwirse report and error and terminate the start prcedure.
	if (m_hPollGroup == k_HSteamNetPollGroup_Invalid) {
		ERR_FAIL_MSG(vformat("Failed to listen on port %d (invalid poll group)", port));
		return;
	}

	//Start the main server loop
	m_runLoop = true;
	m_runLoopThread = std::thread(&World::run_loop, this);

	//Indicate that the world is acting as a server, not a client
	GDNet::get_singleton()->m_isServer = true;

	//TEMP: confirm that the server has started on the requested port:
	print_line(vformat("Server has started on port %d!", port));
	
}

void World::stop_world() {
	m_runLoop = false;

	if (m_runLoopThread.joinable()) {
		m_runLoopThread.join();
	}
		

	//Close the socket
	SteamNetworkingSockets()->CloseListenSocket(m_hListenSock);
	m_hListenSock = k_HSteamListenSocket_Invalid;

	//Destroy poll group
	SteamNetworkingSockets()->DestroyPollGroup(m_hPollGroup);
	m_hPollGroup = k_HSteamNetPollGroup_Invalid;

	//Indicate that the world is no longer acting as the server
	GDNet::get_singleton()->m_isServer = false;
}

void World::join_world(String world, int port) {
	if (!GDNet::get_singleton()->m_player.m_isConnectedToWorld) {
		GDNet::get_singleton()->m_player.connect_to_world(world, port);
	}
}

void World::leave_world() {
	if (GDNet::get_singleton()->m_player.m_isConnectedToWorld) {
		GDNet::get_singleton()->m_player.disconnect_from_world();
	}
}

bool World::load_zone(String zoneName) {
	if (GDNet::get_singleton()->m_isServer) {
		print_line("Cannot request to load zone as the world host!");
		return false;
	}

	print_line("Finding Zone...");
	
	for (std::map<ZoneID_t, Zone *>::const_iterator pair = m_registeredZones.begin(); pair != m_registeredZones.end(); ++pair) {
		if (pair->second->get_name() == zoneName) {
			print_line("Zone Found! sending load zone request.");
			ZoneID_t zoneId = pair->second->get_zone_id();
			SteamNetworkingMessage_t *pLoadZoneRequest = create_mini_message(LOAD_ZONE_REQUEST, zoneId, GDNet::get_singleton()->m_player.get_world_connection());
			send_message(GDNet::get_singleton()->m_player.get_world_connection(), pLoadZoneRequest);
			return true;
		}
	}

	return false;
}



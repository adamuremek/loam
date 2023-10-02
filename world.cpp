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
	m_zone_id_counter = 0;
}

World::~World() {
	s_pCallbackInstace = nullptr;
	//When called here, this mehtod does not allow the editor to start for some reason.
	//stop_world();
}

//==Private Methods==//

void World::steam_net_conn_status_changed_wrapper(SteamNetConnectionStatusChangedCallback_t* pInfo) {
	s_pCallbackInstace->on_net_connection_status_changed(pInfo);
}

void World::on_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t* pInfo) {
	//What is the state of the connection?
	switch (pInfo->m_info.m_eState) {

		case k_ESteamNetworkingConnectionState_None:
			//NOTE: Callbacks will be sent here when connections are destoryed. These can be ignored
			break;

		case k_ESteamNetworkingConnectionState_ClosedByPeer: {
			print_line("Player has disconnected!");
			remove_player(pInfo->m_hConn);
			break;
		}

		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
			break;

		case k_ESteamNetworkingConnectionState_Connecting: {
			print_line("Player is connecting...");

			//Make sure the connecting player isnt already connected (or isnt already int he playerconnections map)
			if (m_worldPlayerConnections.find(pInfo->m_hConn) != m_worldPlayerConnections.end()) {
				break;
			}

			//A client is attempting to connect
			//Try to accept the connection
			if (SteamNetworkingSockets()->AcceptConnection(pInfo->m_hConn) != k_EResultOK) {
				// This could fail.  If the remote host tried to connect, but then
				// disconnected, the connection may already be half closed.  Just
				// destroy whatever we have on our side.
				SteamNetworkingSockets()->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
				print_line("Can't accept connection.  (It was already closed?)");
				break;
			}

			// Assign the poll group
			if (!SteamNetworkingSockets()->SetConnectionPollGroup(pInfo->m_hConn, m_hPollGroup)) {
				SteamNetworkingSockets()->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
				print_line("Failed to set poll group?");
				break;
			}

			break;
		}

		case k_ESteamNetworkingConnectionState_Connected: {
			// We will get a callback immediately after accepting the connection.

			//Make sure the connecting player isnt already connected (i have no idea if/why it would here, but just in case :D )
			if (m_worldPlayerConnections.find(pInfo->m_hConn) != m_worldPlayerConnections.end()) {
				break;
			}

			//Populate player info
			PlayerConnectionInfo player;
			player.m_hConn = pInfo->m_hConn;
			player.id = generate_player_id();

			//Send the player their ID
			SteamNetworkingMessage_t *idAssignmentMssg = create_player_id_assignment_message(player);
			send_message(player, idAssignmentMssg);

			//Add player to list of active player connections
			m_worldPlayerConnections.insert({ pInfo->m_hConn, player });

			print_line("Player has connected!");

			break;
		}
			
		default:
			//No mans land
			//God knows what the hell would end up here. Hopefully nothing important
			break;
	}
}

void World::run_loop() {
	while (m_runLoop) {
		poll_incoming_messages();
		SteamNetworkingSockets()->RunCallbacks();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void World::poll_incoming_messages() {
	while (m_runLoop) {
		ISteamNetworkingMessage *pIncomingMessage = nullptr;
		int numMsgs = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(m_hPollGroup, &pIncomingMessage, 1);

		if (numMsgs == 0)
			break;

		if (numMsgs < 0) {
			ERR_FAIL_MSG("Error checking messages");
			m_runLoop = false;
			return;
		}

		//Dispose of the message
		pIncomingMessage->Release();
	}
}

void World::remove_player(HSteamNetConnection hConn) {
	//Remove the player info from the map if they exist
	if (m_worldPlayerConnections.find(hConn) != m_worldPlayerConnections.end()) {
		m_worldPlayerConnections.erase(hConn);
	}
		

	//Close connection witht the player
	if (!SteamNetworkingSockets()->CloseConnection(hConn, 0, nullptr, false)) {
		print_line("Cannot close connection, it was already closed.");
	}
}

//==Protected Mehtods==//

void World::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start_world", "port"), &World::start_world);
	ClassDB::bind_method(D_METHOD("stop_world"), &World::stop_world);
	ClassDB::bind_method(D_METHOD("join_world", "world", "port"), &World::join_world);
	ClassDB::bind_method(D_METHOD("leave_world"), &World::leave_world);

	ADD_SIGNAL(MethodInfo("world_initialized"));
}


void World::_notification(int n_type) {
	switch (n_type) {
		case NOTIFICATION_READY: {
			register_zones();
			emit_signal("world_initialized");
			break;
		}
	}
}

//==Public Methods==//

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
}

void World::join_world(String world, int port) {
	if (!m_player.m_isConnectedToWorld) {
		m_player.connect_to_world(world, port);
	}
}

void World::leave_world() {
	if (m_player.m_isConnectedToWorld) {
		m_player.disconnect_from_world();
	}
}

void World::register_zones() {
	for (int i = 0; i < this->get_child_count(); i++) {
		print_line("CUM");
		Node *child = this->get_child(i);

		Zone *zone = Object::cast_to<Zone>(child);

		if (zone) {
			Ref<ZoneInfo> zoneInfo(memnew(ZoneInfo));

			int zoneId = m_zone_id_counter;
			m_zone_id_counter++;

			zoneInfo->set_zone_id(zoneId);
			zoneInfo->set_zone_name(zone->get_name());

			zone->set_zone_info(zoneInfo);
		}
	}
}

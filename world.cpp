#include "core/core_string_names.h"
#include "core/error/error_macros.h"
#include "core/os/memory.h"
#include "core/string/print_string.h"
#include "gdnet.h"
#include "include/steam/isteamnetworkingsockets.h"
#include "include/steam/isteamnetworkingutils.h"
#include <thread>

//===============World Implementation===============//

World::World() {
	m_hListenSock = k_HSteamListenSocket_Invalid;
	m_hPollGroup = k_HSteamNetPollGroup_Invalid;
	m_worldConnection = k_HSteamNetConnection_Invalid;
	m_serverRunLoop = false;
	m_clientRunLoop = false;
}

World::~World() {}

void World::cleanup(){
	//Cleanup the client if there exists a client connetion
	if(GDNet::singleton->m_isClient || m_worldConnection != k_HSteamNetConnection_Invalid){
		leave_world();
	}
}

//=======================================GAMENETWORKINGSOCKETS STUFF - SERVER SIDE=======================================//
void World::SERVER_SIDE_CONN_CHANGE(SteamNetConnectionStatusChangedCallback_t *pInfo) {
	GDNet::singleton->world->SERVER_SIDE_connection_status_changed(pInfo);
}

void World::player_connecting(HSteamNetConnection playerConnection) {
	print_line("Player is connecting...");

	// Make sure the connecting player isnt already connected (isnt already in the playerconnections map)
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
	//Make sure the connecting player isnt already connected (should be very rare, but just in case :D )
	if (m_worldPlayerInfoByConnection.find(playerConnection) != m_worldPlayerInfoByConnection.end()) {
		return;
	}

	//Allocate player info
	Ref<PlayerInfo> playerInfo(memnew(PlayerInfo));

	//Generate a network identifier for the player
	PlayerID_t playerId = IDGenerator::generatePlayerID();

	//Populate player info
	playerInfo->set_player_conn(playerConnection);
	playerInfo->set_player_id(playerId);

	//Send the player their ID
	SteamNetworkingMessage_t *idAssignmentMssg = create_mini_message(ASSIGN_PLAYER_ID, playerId, playerConnection);
	send_message_reliable(idAssignmentMssg);

	//Add player to a map keyed by connection, and a map keyed by id
	m_worldPlayerInfoByConnection.insert(playerConnection, playerInfo);
	m_worldPlayerInfoById.insert(playerId, playerInfo);

	print_line("Player has connected!");
}

void World::player_disconnected(HSteamNetConnection playerConnection) {
	print_line("Player has disconnected!");
	remove_player(playerConnection);
}

void World::remove_player(HSteamNetConnection hConn) {
	//Remove the player info from the connection keyed map and the id keyed map if they exist
	if (m_worldPlayerInfoByConnection.has(hConn)) {
		//Assign found value to a variable
		Ref<PlayerInfo> playerInfo = m_worldPlayerInfoByConnection.get(hConn);
		PlayerID_t  playerID = playerInfo->get_player_id();

		//Try to get te zone the player is currently loaded into:
		Zone *currentZone = playerInfo->get_current_loaded_zone();

		//If the player is indeed in a zone, remove them from said zone
		if(currentZone){
			ZoneID_t zoneID = currentZone->get_zone_id();
			currentZone->call_deferred("_remove_player", playerInfo);


			//Tell remaining players in zone to remove the player locally
			for(const KeyValue<PlayerID_t, Ref<PlayerInfo>> &playerInZone : currentZone->m_playersInZone){
				//Skip the leaving player in case they are encountered
				if(playerInZone.key == playerID){
					continue;
				}

				HSteamNetConnection playerEndpoint = playerInZone.value->get_player_conn();

				SteamNetworkingMessage_t* playerLeftMssg = create_small_message(PLAYER_LEFT_ZONE, playerID, zoneID, playerEndpoint);
				send_message_reliable(playerLeftMssg);
			}
		}

		m_worldPlayerInfoById.erase(playerInfo->get_player_id());
		m_worldPlayerInfoByConnection.erase(hConn);
	}

	//Close connection with the player
	if (!SteamNetworkingSockets()->CloseConnection(hConn, 0, nullptr, false)) {
		ERR_PRINT("Cannot close connection, it was already closed.");
	}

	print_line("Connection with a player has been closed.");
}


void World::SERVER_SIDE_load_zone_request(const unsigned char *mssgData, HSteamNetConnection sourceConn) {
	print_line("Load Zone Request recieved!");
	// Get the zone requested by the player
	ZoneID_t zoneId = deserialize_mini(mssgData);
	Zone *zone = GDNet::singleton->m_zoneRegistry.get(zoneId).zone;

	// Try to instantiate the zone (if it hasnt already been)
	zone->call_deferred("instantiate_callback");
//	if (!zone->is_instantiated() && !zone->instantiate_zone()) {
//		ERR_PRINT("Could not instantiate zone!");
//		return;
//	}

	// Tell the player to load the zone locally on their end
	SteamNetworkingMessage_t *pOutgoingMssg = create_mini_message(LOAD_ZONE_REQUEST, zoneId, sourceConn);
	send_message_reliable(pOutgoingMssg);
}

void World::SERVER_SIDE_load_zone_acknowledge(const unsigned char *mssgData, HSteamNetConnection sourceConn) {
	print_line("Load Zone Ack recieved!");
	// Get the requesting player's id and the zone they requested
	Ref<PlayerInfo> playerInfo = m_worldPlayerInfoByConnection[sourceConn];
	ZoneID_t zoneId = deserialize_mini(mssgData);
	Zone *zone = GDNet::singleton->m_zoneRegistry[zoneId].zone;

	// Add the player's info to the zone and add the zone to the player's list of loaded zones.
	zone->add_player(playerInfo);

	//If the laoding player is the only one in the zone, just advance to loading the entitites.
	//Otherwise load in the players currently in the zone first then load the entities.
	if(zone->m_playersInZone.size() == 1){
		//This is important: mark that the player has loaded in all players (since there are none in the zone).
		//Otherwise the server will make this player load in all entities in the zone again when another player loads into the zone.
		playerInfo->m_playerInfo.loadedPlayersInZone = true;

		//Now make the player load all entities that are currently in the zone
		playerInfo->load_entities_in_current_zone();
	}else{
		//Tell all current players in the zone that a new player in loading in.
		//Also tell the new player to load in all other players in the zone.
		//The loading player should not be in the zone yet server side, so we dont have to worry
		//about checking that.
		for(const KeyValue<PlayerID_t, Ref<PlayerInfo>> &playerInZone : zone->m_playersInZone){
			//Skip the playing being loaded so they dont load themselves
			if(playerInZone.value == playerInfo){
				print_line("Skiping current player :#");
				continue;
			}
			//Tell the selected player in the zone to load info about the new player
			playerInZone.value->load_player(playerInfo);

			//Make the new player load the info for the selected player
			playerInfo->load_player(playerInZone.value);
		}
	}
}

void World::SERVER_SIDE_create_zone_player_info_acknowledge(const unsigned char *mssgData, HSteamNetConnection sourceConn) {
	//Get the player who sent the acknowledgement
	Ref<PlayerInfo> playerInfo = m_worldPlayerInfoByConnection.get(sourceConn);

	//Deserialize the acknowledged player (info) id from the message data
	PlayerID_t playerIdAck = deserialize_mini(mssgData);

	//Confirm that the player info has been created on the client's end
	playerInfo->confirm_player_load(playerIdAck);
}

void World::SERVER_SIDE_load_entity_request(const unsigned char *mssgData) {
	print_line("Create entity request recieved!");
	//Create a new entity info refrence to store on the server side
	Ref<EntityInfo> entityInfo(memnew(EntityInfo));

	//Deseralize the message data into the reference
	entityInfo->deserialize_info(mssgData);

	//Create the entity
	ZoneID_t parentZoneId = entityInfo->m_entityInfo.parentZone;
	Zone* parentZone = GDNet::singleton->m_zoneRegistry.get(parentZoneId).zone;

	parentZone->load_entity(entityInfo);

	print_line("Entity load complete!");
}

void World::SERVER_SIDE_load_entity_acknowledge(const unsigned char *mssgData, HSteamNetConnection sourceConn) {
	//Get the player who sent the acknowledgement
	Ref<PlayerInfo> playerInfo = m_worldPlayerInfoByConnection.get(sourceConn);

	//Deserialize the acknowledgement message
	EntityNetworkID_t networkIdAck = deserialize_mini(mssgData);

	//Confirm by recording that the entity has been created successfully on the client's side
	playerInfo->confirm_entity_load(networkIdAck);
}

void World::SERVER_SIDE_handle_entity_update(const unsigned char *mssgData, const int mssgLen) {
	//Get the update information metadata
	EntityUpdateInfo_t updateInfo = NetworkModule::deserialize_update_metadata(mssgData, mssgLen);

	//Send the update information to the corresponding network entity and module
	Zone* parentZone = GDNet::singleton->m_zoneRegistry.get(updateInfo.parentZone).zone;
	Ref<EntityInfo> networkEntity = parentZone->m_entitiesInZone.get(updateInfo.networkId);
	networkEntity->m_entityInfo.entityInstance->SERVER_SIDE_recieve_data(updateInfo);
}

void World::SERVER_SIDE_player_left_zone(const unsigned char *mssgData){
	PlayerID_t leavingPlayer;
	ZoneID_t zoneLeft;

	//Get the player ID and zone ID of the zone that the player is leaving some
	deserialize_small(mssgData, leavingPlayer, zoneLeft);
	print_line(vformat("LEAVING PLAYER DATA: PID %d ZID %d", leavingPlayer, zoneLeft));
	//Get the zone object being left
	Zone* targetZone = GDNet::singleton->m_zoneRegistry.get(zoneLeft).zone;
	//Get the player to remove from zone
	Ref<PlayerInfo> player = targetZone->get_player(leavingPlayer);

	//Remove the player from the zone
	targetZone->call_deferred("_remove_player", player);

	//Tell remaining players in zone to remove the player locally
	for(const KeyValue<PlayerID_t, Ref<PlayerInfo>> &playerInZone : targetZone->m_playersInZone){
		//Skip the leaving player in case they are encountered
		if(playerInZone.key == leavingPlayer){
			continue;
		}

		HSteamNetConnection playerEndpoint = playerInZone.value->get_player_conn();

		SteamNetworkingMessage_t* playerLeftMssg = create_small_message(PLAYER_LEFT_ZONE, leavingPlayer, zoneLeft, playerEndpoint);
		send_message_reliable(playerLeftMssg);
	}

}


void World::SERVER_SIDE_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *pInfo) {
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

void World::SERVER_SIDE_poll_incoming_messages() {
	while (m_serverRunLoop) {
		SteamNetworkingMessage_t *pIncomingMsgs[16];
		int numMsgs = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(m_hPollGroup, pIncomingMsgs, 16);

		if (numMsgs == 0) {
			break;
		}

		if (numMsgs < 0) {
			ERR_PRINT("Error checking messages");
			m_serverRunLoop = false;
			return;
		}

		//Evaluate each message
		for (int i = 0; i < numMsgs; i++) {
			SteamNetworkingMessage_t *pMessage = pIncomingMsgs[i];
			const unsigned char *mssgData = static_cast<unsigned char *>(pMessage->m_pData);

			//Check the type of message recieved
			switch (mssgData[0]) {
				case LOAD_ZONE_REQUEST:
					SERVER_SIDE_load_zone_request(mssgData, pMessage->m_conn);
					break;
				case LOAD_ZONE_ACKNOWLEDGE:
					SERVER_SIDE_load_zone_acknowledge(mssgData, pMessage->m_conn);
					break;
				case CREATE_ZONE_PLAYER_INFO_ACKNOWLEDGE:
					SERVER_SIDE_create_zone_player_info_acknowledge(mssgData, pMessage->m_conn);
					break;
				case CREATE_ENTITY_REQUEST:
					SERVER_SIDE_load_entity_request(mssgData);
					break;
				case CREATE_ENTITY_ACKNOWLEDGE:
					SERVER_SIDE_load_entity_acknowledge(mssgData, pMessage->m_conn);
					break;
				case NETWORK_ENTITY_UPDATE:
					SERVER_SIDE_handle_entity_update(mssgData, pMessage->m_cbSize);
					break;
				case PLAYER_LEFT_ZONE:
					SERVER_SIDE_player_left_zone(mssgData);
					break;
				default:
					break;
			}

			//Dispose of the message
			pMessage->Release();
		}
	}
}

void World::server_listen_loop() {
	while (m_serverRunLoop) {
		SERVER_SIDE_poll_incoming_messages();
		SteamNetworkingSockets()->RunCallbacks();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void World::server_tick_loop() {
	print_line("server tick loop started :)");

	while(m_serverRunLoop){
		emit_signal("_server_side_transmit_entity_data");

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
//=======================================================================================================================//

//=======================================GAMENETWORKINGSOCKETS STUFF - CLIENT SIDE=======================================//
void World::CLIENT_SIDE_CONN_CHANGE(SteamNetConnectionStatusChangedCallback_t *pInfo) {
	GDNet::singleton->world->CLIENT_SIDE_connection_status_changed(pInfo);
}


void World::CLIENT_SIDE_assign_player_id(const unsigned char *mssgData) {
	PlayerID_t id = deserialize_mini(mssgData);
	m_localPlayer->set_player_id(id);
	print_line(vformat("Assigned player id is %d", id));

	//Locally add the local player to the world's list of players in the world (by ID only)
	m_worldPlayerInfoById.insert(id, m_localPlayer);

	//Since this entire loop is running in a different thread from the main thread/game loop,
	//the signal has to be queued to be emitted at the next game loop call.
	call_deferred("emit_signal", "joined_world");
}

void World::CLIENT_SIDE_zone_load_complete(const unsigned char *mssgData) {
	PlayerID_t playerId = deserialize_mini(mssgData);
	print_line(vformat("PLAYER %d HAS FULLY LOADED INTO ZONE", playerId));
}

void World::CLIENT_SIDE_load_zone_request(const unsigned char *mssgData) {
	//Get the zone id that the server wants instantiated
	ZoneID_t zoneId = deserialize_mini(mssgData);
	print_line(vformat("Loading zone with id %d", zoneId));

	//If instantiation was successful, send an acknowledgement to the server
	if(CLIENT_SIDE_instantiate_zone(zoneId)){
		SteamNetworkingMessage_t *zoneLoadAck = create_mini_message(LOAD_ZONE_ACKNOWLEDGE, zoneId, m_worldConnection);
		send_message_reliable(zoneLoadAck);
	}
}

void World::CLIENT_SIDE_process_create_zone_player_info_request(const unsigned char *mssgData) {
	//Get the player ID for the player that needs a player info object to be created
	PlayerID_t playerId = deserialize_mini(mssgData);

	//Create the playerinfo object for the incoming player id and populate it
	Ref<PlayerInfo> incomingPlayerInfo(memnew(PlayerInfo));
	incomingPlayerInfo->set_player_id(playerId);
	print_line(vformat("Created new player info for %d!", playerId));

	//Add the new player info to the zone's list players
	m_localPlayer->get_current_loaded_zone()->add_player(incomingPlayerInfo);

	//Send a "load" acknowledgement back to the server
	SteamNetworkingMessage_t* ackMssg = create_mini_message(CREATE_ZONE_PLAYER_INFO_ACKNOWLEDGE, playerId, m_worldConnection);
	send_message_reliable(ackMssg);
	print_line("Created player and sent ack");
}

void World::CLIENT_SIDE_load_entity_request(const unsigned char *mssgData) {
	print_line("Create entity request recieved!");
	//Create a new entity info refrence to store on the client side
	Ref<EntityInfo> entityInfo(memnew(EntityInfo));

	//Deseralize the message data into the reference
	entityInfo->deserialize_info(mssgData);

	//Create the entity
	ZoneID_t parentZoneId = entityInfo->m_entityInfo.parentZone;
	Zone* parentZone = GDNet::singleton->m_zoneRegistry[parentZoneId].zone;
	parentZone->create_entity(entityInfo);

	//Send entity creation acknowledgement to server
	EntityNetworkID_t networkId = entityInfo->m_entityInfo.networkId;
	HSteamNetConnection worldConn = GDNet::singleton->world->m_worldConnection;

	SteamNetworkingMessage_t* ackMssg = create_mini_message(CREATE_ENTITY_ACKNOWLEDGE, networkId, worldConn);
	send_message_reliable(ackMssg);
}

void World::CLIENT_SIDE_handle_entity_update(const unsigned char *mssgData, const int mssgLen) {
	//Get the update information metadata
	EntityUpdateInfo_t updateInfo = NetworkModule::deserialize_update_metadata(mssgData, mssgLen);

	//Send the update information to the corresponding network entity and module
	Zone* parentZone = GDNet::singleton->m_zoneRegistry.get(updateInfo.parentZone).zone;

	//Make sure the entity was loaded in before trying to apply the update
	if(parentZone->m_entitiesInZone.has(updateInfo.networkId)){
		Ref<EntityInfo> networkEntity = parentZone->m_entitiesInZone.get(updateInfo.networkId);
		networkEntity->m_entityInfo.entityInstance->CLIENT_SIDE_recieve_data(updateInfo);
	}
}

void World::CLIENT_SIDE_player_left_zone(const unsigned char *mssgData) {
	PlayerID_t leavingPlayer;
	ZoneID_t zoneLeft;

	//Get the player ID and zone ID of the zone that the player is leaving some
	deserialize_small(mssgData, leavingPlayer, zoneLeft);
	//Get the zone object being left
	Zone* targetZone = GDNet::singleton->m_zoneRegistry.get(zoneLeft).zone;
	//Get the player to remove from zone
	Ref<PlayerInfo> player = targetZone->get_player(leavingPlayer);
	//Remove the player from the zone
	targetZone->remove_player(player);
}


void World::CLIENT_SIDE_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *pInfo) {
	//What's the state of the connection?
	switch (pInfo->m_info.m_eState) {
		case k_ESteamNetworkingConnectionState_None:
			// NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
			break;

		case k_ESteamNetworkingConnectionState_ClosedByPeer:
			break;
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
			m_clientRunLoop = false;

			// Print an appropriate message
			if (pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connecting) {
				// Note: we could distinguish between a timeout, a rejected connection,
				// or some other transport problem.
				print_line("We sought the remote host, yet our efforts were met with defeat. ", pInfo->m_info.m_szEndDebug);
			} else if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
				print_line("Alas, troubles beset us; we have lost contact with the host. ", pInfo->m_info.m_szEndDebug);
			} else {
				// NOTE: We could check the reason code for a normal disconnection
				print_line("The host hath bidden us farewell.  (%s)", pInfo->m_info.m_szEndDebug);
			}

			// Clean up the connection.  This is important!
			// The connection is "closed" in the network sense, but
			// it has not been destroyed.  We must close it on our end, too
			// to finish up.  The reason information do not matter in this case,
			// and we cannot linger because it's already closed on the other end,
			// so we just pass 0's.
			SteamNetworkingSockets()->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
			m_worldConnection = k_HSteamNetConnection_Invalid;
			break;
		}

		case k_ESteamNetworkingConnectionState_Connecting:
			// We will get this callback when we start connecting.
			print_line("Connecting to world...");
			break;

		case k_ESteamNetworkingConnectionState_Connected:
			print_line("Successfully connected to world!");
			break;

		default:
			// Silences -Wswitch
			break;
	}
}

void World::CLIENT_SIDE_poll_incoming_messages() {
	while (m_clientRunLoop) {
		SteamNetworkingMessage_t *pIncomingMsgs[1];
		int numMsgs = SteamNetworkingSockets()->ReceiveMessagesOnConnection(m_worldConnection, pIncomingMsgs, 1);

		if (numMsgs == 0) {
			return;
		}

		if (numMsgs < 0) {
			ERR_PRINT("Error checking messages");
			m_clientRunLoop = false;
			return;
		}

		//Evaluate each message
		for (int i = 0; i < numMsgs; i++) {
			SteamNetworkingMessage_t *pMessage = pIncomingMsgs[i];
			const unsigned char *mssgData = static_cast<unsigned char *>(pMessage->m_pData);

			//Check the type of message recieved and evaluate accordingly
			switch (mssgData[0]) {
				case ASSIGN_PLAYER_ID:
					CLIENT_SIDE_assign_player_id(mssgData);
					break;
				case LOAD_ZONE_REQUEST:
					CLIENT_SIDE_load_zone_request(mssgData);
					break;
				case LOAD_ZONE_COMPLETE:
					CLIENT_SIDE_zone_load_complete(mssgData);
					break;
				case CREATE_ZONE_PLAYER_INFO_REQUEST:
					CLIENT_SIDE_process_create_zone_player_info_request(mssgData);
					break;
				case CREATE_ENTITY_REQUEST:
					CLIENT_SIDE_load_entity_request(mssgData);
					break;
				case NETWORK_ENTITY_UPDATE:
					CLIENT_SIDE_handle_entity_update(mssgData, pMessage->m_cbSize);
					break;
				case PLAYER_LEFT_ZONE:
					CLIENT_SIDE_player_left_zone(mssgData);
					break;
				default:
					break;
			}

			//Dispose of the message
			pMessage->Release();
		}
	}
}

void World::client_listen_loop() {
	while (m_clientRunLoop) {
		CLIENT_SIDE_poll_incoming_messages();
		SteamNetworkingSockets()->RunCallbacks();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void World::client_tick_loop() {
	print_line("client tick loop started :)");

	while(m_clientRunLoop){
		emit_signal("_client_side_transmit_entity_data");

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

//=======================================================================================================================//

//==Protected Mehtods==//

void World::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start_world", "port"), &World::start_world);
	ClassDB::bind_method(D_METHOD("stop_world"), &World::stop_world);
	ClassDB::bind_method(D_METHOD("get_player_id"), &World::get_player_id);
	ClassDB::bind_method(D_METHOD("join_world", "world", "port"), &World::join_world);
	ClassDB::bind_method(D_METHOD("leave_world"), &World::leave_world);
	ClassDB::bind_method(D_METHOD("load_zone_by_name", "zone_name"), &World::load_zone_by_name);
	ClassDB::bind_method(D_METHOD("load_zone_by_id", "zone_id"), &World::load_zone_by_id);
	ClassDB::bind_method(D_METHOD("unload_zone"), &World::unload_zone);

	ADD_SIGNAL(MethodInfo("joined_world"));
	ADD_SIGNAL(MethodInfo("left_world"));
	ADD_SIGNAL(MethodInfo("loaded_zone", PropertyInfo(Variant::OBJECT, "zone", PROPERTY_HINT_RESOURCE_TYPE, "Node")));

	ADD_SIGNAL(MethodInfo("_client_side_transmit_entity_data"));
	ADD_SIGNAL(MethodInfo("_server_side_transmit_entity_data"));
}

//==Public Methods==//

bool World::CLIENT_SIDE_instantiate_zone(ZoneID_t zoneId) {
	if (GDNet::singleton->m_zoneRegistry.has(zoneId)) {
		Zone* requestedZone = GDNet::singleton->m_zoneRegistry.get(zoneId).zone;
		//Add local player to the zone (locally)
		requestedZone->add_player(m_localPlayer);
		//Load the zone
		requestedZone->call_deferred("instantiate_callback");

		return true;
	} else {
		return false;
	}
}

void World::start_world(int port) {
	if (!GDNet::singleton->m_isInitialized) {
		ERR_PRINT("GDNet is not initialized!");
		return;
	}

	//Define network connection info
	SteamNetworkingIPAddr worldInfo{};
	worldInfo.Clear();
	worldInfo.m_port = (uint16)port;

	//Create a poll group
	m_hPollGroup = SteamNetworkingSockets()->CreatePollGroup();

	//Server Config (set callbacks)
	SteamNetworkingConfigValue_t cfg{};
	cfg.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void *)World::SERVER_SIDE_CONN_CHANGE);

	//Create a listening socket with the server config
	m_hListenSock = SteamNetworkingSockets()->CreateListenSocketIP(worldInfo, 1, &cfg);

	//Make sure the listening socket is valid, otherwise report an error and terminate start procedure.
	if (m_hListenSock == k_HSteamListenSocket_Invalid) {
		ERR_FAIL_MSG(vformat("Failed to listen on port %d (invalid socket)", port));
		return;
	}

	//Make sure the poll group is valid, otherwirse report and error and terminate the start prcedure.
	if (m_hPollGroup == k_HSteamNetPollGroup_Invalid) {
		ERR_FAIL_MSG(vformat("Failed to listen on port %d (invalid poll group)", port));
		return;
	}

	//Start the main server loop
	m_serverRunLoop = true;
	//Start the server listen loop
	m_serverListenThread = std::thread(&World::server_listen_loop, this);
	//Start the server tick loop
	m_serverTickThread = std::thread(&World::server_tick_loop, this);

	//Indicate that the world is acting as a server, not a client
	GDNet::singleton->m_isServer = true;

	//TEMP: confirm that the server has started on the requested port:
	print_line(vformat("Server has started on port %d!", port));
}

void World::stop_world() {
	m_serverRunLoop = false;

	//Stop the listen loop
	if (m_serverListenThread.joinable()) {
		m_serverListenThread.join();
	}

	//Stop the tick loop
	if(m_serverTickThread.joinable()){
		m_serverTickThread.join();
	}

	//Close the socket
	SteamNetworkingSockets()->CloseListenSocket(m_hListenSock);
	m_hListenSock = k_HSteamListenSocket_Invalid;

	//Destroy poll group
	SteamNetworkingSockets()->DestroyPollGroup(m_hPollGroup);
	m_hPollGroup = k_HSteamNetPollGroup_Invalid;

	//Indicate that the world is no longer acting as the server
	GDNet::singleton->m_isServer = false;
}

PlayerID_t World::get_player_id() {
	return m_localPlayer->get_player_id();
}

void World::join_world(String world, int port) {
	//Define network connection info
	SteamNetworkingIPAddr worldInfo{};
	worldInfo.Clear();
	worldInfo.ParseString(world.utf8().get_data());
	worldInfo.m_port = port;

	//Client config (set callbacks)
	SteamNetworkingConfigValue_t cfg{};
	cfg.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void *)World::CLIENT_SIDE_CONN_CHANGE);

	//Connect to world
	m_worldConnection = SteamNetworkingSockets()->ConnectByIPAddress(worldInfo, 1, &cfg);

	//Make sure the player was able to initiate a connection
	if (m_worldConnection == k_HSteamNetConnection_Invalid) {
		ERR_PRINT("Unable to conncect to initiate connection.");
		return;
	}

	//Create local player info object to store info in
	m_localPlayer = Ref<PlayerInfo>(memnew(PlayerInfo));

	//Enable client run loops
	m_clientRunLoop = true;
	//Start the client listen loop
	m_clientListenThread = std::thread(&World::client_listen_loop, this);
	//Start the client transmit loop
	m_clientTickThread = std::thread(&World::client_tick_loop, this);

	//Indicate that the world is acting as a client
	GDNet::singleton->m_isClient = true;
	print_line("Connection to world has initiated.");
}

void World::leave_world() {
	//Make sure client is connected to a world
	if (!GDNet::singleton->m_isClient || m_worldConnection == k_HSteamNetConnection_Invalid){
		ERR_PRINT("Not connected to a world, therefore cannot leave world!");
		return;
	}

	//Check for a zone to unload, and unload it if there is one
	Zone* loadedZone = m_localPlayer->get_current_loaded_zone();
	if(loadedZone){
		//Remove the player from the zone locally
		loadedZone->remove_player(m_localPlayer);

		//Destroy the zone locally (for now)
		loadedZone->uninstantiate_zone();
	}

	m_clientRunLoop = false;

	//Stop the listen loop
	if (m_clientListenThread.joinable()){
		m_clientListenThread.join();
	}

	//Stop the tick loop
	if(m_clientTickThread.joinable()){
		m_clientTickThread.join();
	}

	//Stop world connection
	SteamNetworkingSockets()->CloseConnection(m_worldConnection, 0, nullptr, false);
	m_worldConnection = k_HSteamNetConnection_Invalid;

	GDNet::singleton->m_isClient = false;

	//Inform signal connections that client has left the world
	emit_signal("left_world");
}

bool World::load_zone_by_name(String zoneName) {
	if (GDNet::singleton->m_isServer) {
		print_line("Cannot request to load zone as the world host!");
		return false;
	}

	print_line("Finding Zone...");

	for (const KeyValue<ZoneID_t, ZoneInfo_t> &element : GDNet::singleton->m_zoneRegistry) {
		if (element.value.name == zoneName) {
			print_line("Zone Found! sending load zone request.");
			ZoneID_t zoneId = element.value.id;

			//Make sure the zone has a scene to load
			if(!element.value.zone->get_zone_scene().is_valid()){
				ERR_PRINT(vformat("Zone with ID %d was not given a scene to create!", zoneId));
				return false;
			}

			SteamNetworkingMessage_t *pLoadZoneRequest = create_mini_message(LOAD_ZONE_REQUEST, zoneId, m_worldConnection);
			send_message_reliable(pLoadZoneRequest);
			return true;
		}
	}

	print_line("Could not find zone :(");
	return false;
}

bool World::load_zone_by_id(ZoneID_t zoneId) {
	if (GDNet::singleton->m_isServer) {
		print_line("Cannot request to load zone as the world host!");
		return false;
	}

	if (GDNet::singleton->m_zoneRegistry.has(zoneId)) {
		print_line("Zone Found! sending load zone request.");

		//Make sure the zone has a scene to load
		if(!GDNet::singleton->m_zoneRegistry.get(zoneId).zone->get_zone_scene().is_valid()){
			ERR_PRINT(vformat("Zone with ID %d was not given a scene to create!", zoneId));
			return false;
		}

		SteamNetworkingMessage_t *pLoadZoneRequest = create_mini_message(LOAD_ZONE_REQUEST, zoneId, m_worldConnection);
		send_message_reliable(pLoadZoneRequest);
		return true;
	}else{
		print_line("Zone not found :(");
		return false;
	}
}

void World::unload_zone() {
	if(GDNet::singleton->m_isServer){
		ERR_PRINT("Server cant unload zones for now.");
		return;
	}

	if(!GDNet::singleton->m_isClient){
		ERR_PRINT("Client must be running!");
		return;
	}

	//Get the zone to unload
	Zone* loadedZone = m_localPlayer->get_current_loaded_zone();
	//Make sure zone was even loaded
	if(!loadedZone){
		ERR_PRINT("No zone loaded!");
		return;
	}

	//Tell the server that the player has unloaded the zone
	SteamNetworkingMessage_t* playerLeftMssg = create_small_message(PLAYER_LEFT_ZONE, m_localPlayer->get_player_id(), loadedZone->get_zone_id(), m_worldConnection);
	send_message_reliable(playerLeftMssg);

	loadedZone->remove_player(m_localPlayer);

	//Destroy the zone locally (for now)
	loadedZone->uninstantiate_zone();
}

bool World::player_exists(PlayerID_t playerId) {
	if(!GDNet::singleton->m_isClient && !GDNet::singleton->m_isServer){
		ERR_PRINT("Cannot lookup players since there is no world running and there is no connection to a world.");
		return false;
	}

	return m_worldPlayerInfoById.find(playerId) != m_worldPlayerInfoById.end();
}

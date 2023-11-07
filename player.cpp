#include "gdnet.h"
#include "core/os/memory.h"
#include "core/string/print_string.h"
#include "core/error/error_macros.h"
#include "core/core_string_names.h"
#include "include/steam/isteamnetworkingsockets.h"
#include "include/steam/isteamnetworkingutils.h"
#include <thread>

Player *Player::s_pCallbackInstance = nullptr;


Player::Player() {
	s_pCallbackInstance = this;
	m_world = nullptr;
	m_hConnection = k_HSteamNetConnection_Invalid;
	m_runLoop = false;
	m_isConnectedToWorld = false;

	print_line("poop cock");
}

Player::~Player() {}

//==Private Methods==//

void Player::steam_net_conn_status_changed_wrapper(SteamNetConnectionStatusChangedCallback_t* pInfo) {
	s_pCallbackInstance->on_net_connection_status_changed(pInfo);
}

void Player::run_loop() {
	while (m_runLoop) {
		poll_incoming_messages();
		SteamNetworkingSockets()->RunCallbacks();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void Player::on_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t* pInfo) {
	// What's the state of the connection?
	switch (pInfo->m_info.m_eState) {
		case k_ESteamNetworkingConnectionState_None:
			// NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
			break;

		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
			m_runLoop = true;

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
			m_hConnection = k_HSteamNetConnection_Invalid;
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

void Player::poll_incoming_messages() {
	while (m_runLoop) {
		SteamNetworkingMessage_t *pIncomingMsgs[1];
		int numMsgs = SteamNetworkingSockets()->ReceiveMessagesOnConnection(m_hConnection, pIncomingMsgs, 1);

		if (numMsgs == 0) {
			return;
		}
			
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
				case ASSIGN_PLAYER_ID: {
					PlayerID_t id = deserialize_mini(mssgData);
					print_line(vformat("Assigned player id is %d", id));

					//Since this entire loop is running in a different thread from the main thread/game loop,
					//the signal has to be queued to be emitted at the next game loop call.
					m_world->call_deferred("emit_signal", "joined_world");
					break;
				}
				case LOAD_ZONE_ACCEPT: {
					ZoneID_t zoneId = deserialize_mini(mssgData);
					print_line(vformat("Loading zone with id %d", zoneId));

					m_world->instantiate_zone_by_id(zoneId);

					break;
				}
				case PLAYER_ENTERED_ZONE: {
					PlayerID_t playerId = deserialize_mini(mssgData);
					
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

//==Protected Methods==//

void Player::_bind_methods() {
	//ClassDB::bind_method(D_METHOD("connect_to_world", "world", "port"), &Player::connect_to_world);
}

//==Public Methods==//

void Player::connect_to_world(String world, int port) {
	//Define network connection info
	SteamNetworkingIPAddr worldInfo;
	worldInfo.Clear();
	worldInfo.ParseString(world.utf8().get_data());
	worldInfo.m_port = port;

	//Player config (set callbacks)
	SteamNetworkingConfigValue_t cfg;
	cfg.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void *)Player::steam_net_conn_status_changed_wrapper);

	//Connect to world
	m_hConnection = SteamNetworkingSockets()->ConnectByIPAddress(worldInfo, 1, &cfg);

	//Make sure the player was able to connect to the world
	if (m_hConnection == k_HSteamNetConnection_Invalid) {
		ERR_FAIL_MSG("Unable to conncect to world.");
		return;
	}

	//Star the player main loop
	m_runLoop = true;
	m_runLoopThread = std::thread(&Player::run_loop, this);

	m_isConnectedToWorld = true;
	print_line("Connection to world has initiated.");
		
}

void Player::disconnect_from_world() {
	//Make sure player is connected to a world
	if (!m_isConnectedToWorld)
		return;

	m_runLoop = false;

	//Stop the run loop
	if (m_runLoopThread.joinable())
		m_runLoopThread.join();

	//Stop world connection
	SteamNetworkingSockets()->CloseConnection(m_hConnection, 0, nullptr, false);
	m_hConnection = k_HSteamNetConnection_Invalid;

	m_isConnectedToWorld = false;
}

HSteamNetConnection Player::get_world_connection() {
	return m_hConnection;
}


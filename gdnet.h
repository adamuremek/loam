#ifndef GDNET_SERVER_H
#define GDNET_SERVER_H

#include "scene/main/node.h"
#include "include/steam/steamnetworkingsockets.h"
#include <thread>
#include <map>



struct PlayerConnectionInfo {
	//Connection handle (id)
	HSteamNetConnection m_hConn;
};


class NetworkManager : public Object {
	GDCLASS(NetworkManager, Object);

public:
	void JoinWorld();
};

//===============World===============//

class World : public Node {
	GDCLASS(World, Node);

private:
	static World *s_pCallbackInstace;
	ISteamNetworkingSockets *m_pInterface;
	std::thread m_recieveLoopThread;
	HSteamNetPollGroup m_hPollGroup;
	HSteamListenSocket m_hListenSock;
	std::map<HSteamNetConnection, PlayerConnectionInfo> m_worldPlayerConnections;
	bool m_serverLoop = false;

	static void steam_net_conn_status_changed_wrapper(SteamNetConnectionStatusChangedCallback_t *pInfo);

	void recieve_run_loop();
	void on_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *pCallback);
	void poll_incoming_messages();

	void remove_player(HSteamNetConnection hConn);


protected:
	static void _bind_methods();

public:
	World();
	~World();

	void start_world(int port);
	void stop_world();
};


//===============GDNetClient===============//
class GDNetClient : public Object {
	GDCLASS(GDNetClient, Object);

private:

protected:
	static void _bind_methods();

public:
	GDNetClient();
	~GDNetClient();

	void connect_to_server(String host, int port);
};
#endif

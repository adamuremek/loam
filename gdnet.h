#ifndef GDNET_SERVER_H
#define GDNET_SERVER_H

#include "scene/main/node.h"
#include "include/steam/steamnetworkingsockets.h"
#include <thread>


//===============GDNetServer===============//

class GDNetServer : public Node {
	GDCLASS(GDNetServer, Node);

private:
	static GDNetServer *s_pCallbackInstace;
	ISteamNetworkingSockets *m_pInterface;
	std::thread m_recieveLoopThread;
	HSteamNetPollGroup m_hPollGroup;
	HSteamListenSocket m_hListenSock;
	bool m_serverLoop = false;

	static void steam_net_conn_status_changed_wrapper(SteamNetConnectionStatusChangedCallback_t *pInfo);

	void recieve_run_loop();
	void on_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *pCallback);
	void poll_incoming_messages();


protected:
	static void _bind_methods();

public:
	GDNetServer();
	~GDNetServer();

	void start_server(int port);
	void stop_server();
};


//===============GDNetClient===============//
class GDNetClient : public Node {
	GDCLASS(GDNetClient, Node);

private:

protected:
	static void _bind_methods();

public:
	GDNetClient();
	~GDNetClient();

	void connect_to_server(String host, int port);
};
#endif

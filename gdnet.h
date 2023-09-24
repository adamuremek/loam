#ifndef GDNET_SERVER_H
#define GDNET_SERVER_H

#include "scene/main/node.h"
#include "modules/enet/enet_connection.h"
#include "modules/enet/enet_packet_peer.h"


//===============GDNetServer===============//

class GDNetServer : public Node {
	GDCLASS(GDNetServer, Node);

private:
	Ref<ENetConnection> server;

protected:
	static void _bind_methods();

public:
	GDNetServer();
	~GDNetServer();

	void start_server(int port);
};


//===============GDNetClient===============//
class GDNetClient : public Node {
	GDCLASS(GDNetClient, Node);

private:
	ENetPacketPeer *peer;

protected:
	static void _bind_methods();

public:
	GDNetClient();
	~GDNetClient();

	void connect_to_server(String host, int port);
};
#endif

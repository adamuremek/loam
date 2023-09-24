#include "gdnet.h"
#include "core/os/memory.h"
#include "core/core_string_names.h"
#include "modules/enet/enet_connection.h"
#include "modules/enet/enet_packet_peer.h"


//===============GDNetServer Implementation===============//

GDNetServer::GDNetServer() {
	server = nullptr;
}

GDNetServer::~GDNetServer() {
	if (server) {
		enet_host_destroy(server);
	}
}

void GDNetServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start_server", "port"), &GDNetServer::start_server);
}

void GDNetServer::start_server(int port) {
	server = memnew()

	if (!server) {
		print_error("Failed to start GDNet server.");
		return;
	}
}

//===============GDNetClient Implementation===============//

GDNetClient::GDNetClient() {
	peer = nullptr;
}

GDNetClient::~GDNetClient() {
	if (peer) {
		enet_peer_reset(peer);
	}
}

void GDNetClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("connect_to_server", "host", "port"), &GDNetClient::connect_to_server);
}

void GDNetClient::connect_to_server(String host, int port) {
	ENetHost *client;
	client = enet_host_create(NULL, 1, 2, 0, 0);

	if (!client) {
		print_error("Failed to create GDNet client.");
		return;
	}

	ENetAddress address;
	enet_address_set_host(&address, host.utf8().get_data());
	address.port = port;
	peer = enet_host_connect(client, &address, 2, 0);
}


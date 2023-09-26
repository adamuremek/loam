#include "gdnet.h"
#include "core/os/memory.h"
#include "core/core_string_names.h"
#include <steam/steamnetworkingsockets.h>


//===============GDNetServer Implementation===============//

GDNetServer::GDNetServer() {

}

GDNetServer::~GDNetServer() {

}

void GDNetServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start_server", "port"), &GDNetServer::start_server);
}

void GDNetServer::start_server(int port) {

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


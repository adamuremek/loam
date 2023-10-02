#include "gdnet.h"
#include "include/steam/steamnetworkingsockets.h"

GDNet *GDNet::s_singleton = nullptr;

GDNet::GDNet() {
	s_singleton = this;
	m_isInitialized = false;
}

GDNet::~GDNet() {
	s_singleton = nullptr;
}

void GDNet::_bind_methods() {
	ClassDB::bind_method("init_gdnet", &GDNet::init_gdnet);
	ClassDB::bind_method("shutdown_gdnet", &GDNet::shutdown_gdnet);
}


GDNet *GDNet::get_singleton() {
	return s_singleton;
}

void GDNet::init_gdnet() {
	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
		ERR_FAIL_MSG("Could not start ");
		return;
	}
		
	m_isInitialized = true;
}

void GDNet::shutdown_gdnet() {
	GameNetworkingSockets_Kill();

	m_isInitialized = false;
}

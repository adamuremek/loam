#ifndef GDNET_SERVER_H
#define GDNET_SERVER_H

#include "scene/main/node.h"
#include "core/object/ref_counted.h"
#include "scene/resources/packed_scene.h"
#include "include/steam/isteamnetworkingsockets.h"
#include <thread>
#include <map>


//===============Data and Types===============//
//Control Message Types
#define ASSIGN_PLAYER_ID 1

typedef unsigned short PlayerID_t;
typedef unsigned short ZoneID_t;
static unsigned short id_counter = 1;

struct PlayerConnectionInfo {
	//Connection handle (id)
	HSteamNetConnection m_hConn;
	PlayerID_t id;
};

//===============Messaging===============//

PlayerID_t generate_player_id();
SteamNetworkingMessage_t *create_player_id_assignment_message(const PlayerConnectionInfo& player);
void send_message(const PlayerConnectionInfo &player, SteamNetworkingMessage_t *message);

PlayerID_t deserialize_id(const char *data, int startIdx);

//===============GDNet Singleton===============//

class GDNet : public RefCounted {
	GDCLASS(GDNet, RefCounted);

protected:
	static GDNet *s_singleton;
	static void _bind_methods();

public:
	bool m_isInitialized;

	GDNet();
	~GDNet();

	static GDNet* get_singleton();

	void init_gdnet();
	void shutdown_gdnet();
};

//===============Player===============//

class Player : public Object {
	GDCLASS(Player, Object);

private:
	static Player *s_pCallbackInstance;
	HSteamNetConnection m_hConnection;
	std::thread m_runLoopThread;
	bool m_runLoop;

	static void steam_net_conn_status_changed_wrapper(SteamNetConnectionStatusChangedCallback_t *pInfo);

	void run_loop();
	void on_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *pInfo);
	void poll_incoming_messages();

protected:
	static void _bind_methods();

public:
	bool m_isConnectedToWorld;

	Player();
	~Player();

	void connect_to_world(String host, int port);

	void disconnect_from_world();
};

//===============ZoneInfo===============//

class ZoneInfo : public RefCounted {
	GDCLASS(ZoneInfo, RefCounted);

private:
	int m_zoneId;
	String m_zoneName;

protected:
	static void _bind_methods();

public:
	ZoneInfo();

	int get_zone_id();
	void set_zone_id(int zoneId);
	String get_zone_name();
	void set_zone_name(String zoneName);
};

//===============Zone===============//

class Zone : public Node {
	GDCLASS(Zone, Node);

private:
	Ref<PackedScene> m_zoneScene;
	Ref<ZoneInfo> m_zoneInfo;

protected:
	static void _bind_methods();

public:
	Zone();
	~Zone();

	Ref<PackedScene> get_zone_scene() const;
	void set_zone_scene(const Ref<PackedScene> &zoneScene);
	Ref<ZoneInfo> get_zone_info() const;
	void set_zone_info(const Ref<ZoneInfo> &zoneInfo);
};


//===============World===============//

class World : public Node {
	GDCLASS(World, Node);

private:
	//Members relavent when acting as server
	static World *s_pCallbackInstace;
	std::thread m_runLoopThread;
	HSteamNetPollGroup m_hPollGroup;
	HSteamListenSocket m_hListenSock;
	std::map<HSteamNetConnection, PlayerConnectionInfo> m_worldPlayerConnections;
	bool m_runLoop;

	int m_zone_id_counter;
	
	//Members relavent when acting as client
	Player m_player;

	static void steam_net_conn_status_changed_wrapper(SteamNetConnectionStatusChangedCallback_t *pInfo);

	void run_loop();
	void on_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *pInfo);
	void poll_incoming_messages();

	void remove_player(HSteamNetConnection hConn);

	void register_zones();

protected:
	static void _bind_methods();

	void _notification(int n_type);

public:
	World();
	~World();

	void start_world(int port);
	void stop_world();

	void join_world(String world, int port);
	void leave_world();
};

#endif

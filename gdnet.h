#ifndef GDNET_H
#define GDNET_H

#include "scene/main/node.h"
#include "core/object/ref_counted.h"
#include "scene/resources/packed_scene.h"
#include "include/steam/isteamnetworkingsockets.h"
#include <thread>
#include <map>
#include <stack>
#include <unordered_set>
#include <vector>

//===============Data and Types===============//
//Control Message Types
#define ASSIGN_PLAYER_ID 1
#define LOAD_ZONE_REQUEST 2
#define LOAD_ZONE_ACCEPT 3
#define LOAD_ZONE_DENY 4
#define PLAYER_ENTERED_ZONE 5
#define PLAYER_LEFT_ZONE 6
#define INSTANTIATE_NETWORK_ENTITY_REQUEST 7 
#define INSTANTIATE_NETWORK_ENTITY 8

typedef unsigned int PlayerID_t;
typedef unsigned int EntityNetworkID_t;
typedef unsigned int EntityID_t;
typedef unsigned short ZoneID_t;

struct PlayerConnectionInfo {
	//Connection handle (id)
	HSteamNetConnection m_hConn;
	PlayerID_t id;
};

//===============Forward Declarations===============//
class Zone;
class Player;
class World;

//===============Messaging===============//
SteamNetworkingMessage_t *create_mini_message(const int messageType, unsigned int value, const HSteamNetConnection &destination);
SteamNetworkingMessage_t *create_small_message(const int messageType, unsigned int value1, unsigned int value2, const HSteamNetConnection &destination);
unsigned int deserialize_mini(const char *data);
void deserialize_small(const char *data, unsigned int &value1, unsigned int &value2);

void send_message(const HSteamNetConnection &destination, SteamNetworkingMessage_t *message);


//===============ID Generator===============//

class IDGenerator {
private:
	static std::stack<unsigned int> freePlayerIDs;
	static std::stack<unsigned int> freeNetworkEntityIDs;
	static std::unordered_set<unsigned int> usedPlayerIDs;
	static std::unordered_set<unsigned int> usedNetworkEntityIDs;

	static unsigned int s_playerIDCounter;
	static unsigned int s_networkEntityIDCounter;

public:
	static PlayerID_t generatePlayerID();
	static EntityNetworkID_t generateNetworkIdentityID();
	static void freePlayerID(PlayerID_t playerID);
	static void freeNetworkEntityID(EntityNetworkID_t networkEntityID);
};


//===============GDNet Singleton===============//

class GDNet : public RefCounted {
	GDCLASS(GDNet, RefCounted);

private:
	/**
	Load network entity scenes into a data structure from the NETWORK_ENTITIES directory.
	*/
	bool load_network_entities();

protected:
	static Ref<GDNet> s_singleton;
	static void _bind_methods();

public:
	bool m_isInitialized;
	bool m_isClient;
	bool m_isServer;
	Player m_player;
	std::map<EntityID_t, Ref<PackedScene>> m_entitiesById;
	std::map<String, EntityID_t> m_entityIdByName;

	GDNet();
	~GDNet();

	static Ref<GDNet> get_singleton();

	void init_gdnet();
	void shutdown_gdnet();

	bool is_client();
	bool is_server();
};

//===============Sync Transform===============//
class SyncTransform : public Node {
	GDCLASS(SyncTransform, Node);

protected:
	static void _bind_methods();
};


//===============Network Entity===============//

class NetworkEntity : public Node {
	GDCLASS(NetworkEntity, Node);

private:

protected:
	static void _bind_methods();
	void _notification(int n_type);

public:

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

	World *m_world;

	void connect_to_world(String host, int port);
	void disconnect_from_world();

	HSteamNetConnection get_world_connection();
};

//===============Zone===============//

class Zone : public Node {
	GDCLASS(Zone, Node);

private:
	static ZoneID_t m_zone_id_counter;

	World *m_parentWorld;
	Node *m_zoneInstance;
	Ref<PackedScene> m_zoneScene;
	ZoneID_t m_zoneId;

protected:
	static void _bind_methods();
	void _notification(int n_type);

public:
	Zone();
	~Zone();

	bool m_instantiated;
	std::vector<PlayerID_t> m_playersInZone;

	Ref<PackedScene> get_zone_scene() const;
	void set_zone_scene(const Ref<PackedScene> &zoneScene);
	ZoneID_t get_zone_id() const;
	void instantiate_zone();

	void add_player(PlayerID_t playerID);
	void create_network_entity(String entityName);
};


//===============World===============//

class World : public Node {
	GDCLASS(World, Node);

private:
	//Relavent when acting as server
	static World *s_pCallbackInstace;
	std::thread m_runLoopThread;
	HSteamNetPollGroup m_hPollGroup;
	HSteamListenSocket m_hListenSock;
	std::map<HSteamNetConnection, PlayerConnectionInfo> m_worldPlayerInfoByConnection;
	std::map<PlayerID_t, PlayerConnectionInfo> m_worldPlayerInfoById;
	std::map<ZoneID_t, Zone *> m_registeredZones;
	bool m_runLoop;

	static void steam_net_conn_status_changed_wrapper(SteamNetConnectionStatusChangedCallback_t *pInfo);

	void player_connecting(HSteamNetConnection playerConnection);
	void player_connected(HSteamNetConnection playerConnection);
	void player_disconnected(HSteamNetConnection playerConnection);

	void run_loop();
	void on_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *pInfo);
	void poll_incoming_messages();

	void remove_player(HSteamNetConnection hConn);

protected:
	static void _bind_methods();
	void _notification(int n_type);

public:
	World();
	~World();

	void register_zone(Zone *zone);
	void unregister_zone(Zone *zone);
	bool instantiate_zone_by_id(ZoneID_t zoneId);

	void start_world(int port);
	void stop_world();

	void join_world(String world, int port);
	void leave_world();

	bool load_zone(String zoneName);
	void unload_zone();
};

#endif

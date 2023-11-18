#ifndef GDNET_H
#define GDNET_H

#include "core/object/ref_counted.h"
#include "include/steam/isteamnetworkingsockets.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"
#include <algorithm>
#include <cstdint>
#include <map>
#include <stack>
#include <thread>
#include <unordered_set>
#include <vector>

//===============Data and Types===============//
//Control Message Types (these are basically events that are fired across the network)
#define ASSIGN_PLAYER_ID static_cast<unsigned char>(0x01)

#define LOAD_ZONE_REQUEST static_cast<unsigned char>(0x02)
#define LOAD_ZONE_DENY static_cast<unsigned char>(0x03)
#define LOAD_ZONE_ACKNOWLEDGE static_cast<unsigned char>(0x04)
#define LOAD_ZONE_COMPLETE static_cast<unsigned char>(0x05)

#define PLAYER_ENTERED_ZONE static_cast<unsigned char>(0x06)
#define PLAYER_LEFT_ZONE static_cast<unsigned char>(0x07)

#define CREATE_ENTITY_REQUEST static_cast<unsigned char>(0x08)
#define CREATE_ENTITY_DENY static_cast<unsigned char>(0x09)
#define CREATE_ENTITY_ACKNOWLEDGE static_cast<unsigned char>(0x0A)
#define CREATE_ENTITY_COMPLETE static_cast<unsigned char>(0x0B)

typedef uint32_t PlayerID_t;
typedef uint32_t EntityNetworkID_t;
typedef uint32_t EntityID_t;
typedef uint32_t ZoneID_t;
typedef uint8_t MessageType_t;

//Forward declarations
class Zone;
class World;
class EntityInfo;
class NetworkEntity;
struct ZoneInfo;

struct PlayerInfo {
	//Connection handle
	HSteamNetConnection m_hConn;
	PlayerID_t id;
	//std::vector<ZoneInfo> loadedZones;
	std::map<ZoneID_t, std::vector<Ref<EntityInfo>>> associatedEntities;
};

struct NetworkEntityInfo {
	EntityID_t id;
	String name;
	Ref<PackedScene> scene;
};

struct ZoneInfo {
	ZoneID_t id;
	String name;
	Zone *zone;
};

struct EntityInfo_t {
	String entityName;
	String parentRelativePath;
	ZoneID_t parentZone;
	EntityID_t entityId;
	EntityNetworkID_t networkId;
	PlayerID_t associatedPlayer;
	std::vector<unsigned char> dataBuffer;

	NetworkEntity *entityInstance;
	std::vector<PlayerID_t> creationAcks;
};

//===============Messaging===============//
SteamNetworkingMessage_t *allocate_message(const unsigned char *data, const int sizeOfData, const HSteamNetConnection &destination);
SteamNetworkingMessage_t *create_mini_message(MessageType_t messageType, unsigned int value, const HSteamNetConnection &destination);
SteamNetworkingMessage_t *create_small_message(MessageType_t messageType, unsigned int value1, unsigned int value2, const HSteamNetConnection &destination);
//SteamNetworkingMessage_t *instantiate_entity_message(const EntityID_t entityID, String parentNode, const HSteamNetConnection &destination);

void serialize_int(int value, int startIdx, unsigned char *buffer);
void serialize_uint(unsigned int value, int startIdx, unsigned char *buffer);
void serialize_string();
unsigned int deserialize_uint(int startIdx, const unsigned char *buffer);
int deserialize_int(int startIdx, const unsigned char *buffer);
String deserialize_string(int startIdx, int stringLength, const unsigned char *buffer);
void copy_string_to_buffer(const char *string, unsigned char *buffer, int startIDx, int stringSize);

unsigned int deserialize_mini(const unsigned char *data);
void deserialize_small(const char *data, unsigned int &value1, unsigned int &value2);

void send_message(SteamNetworkingMessage_t *message);

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

//===============Entity Info===============//

class EntityInfo : public RefCounted {
	GDCLASS(EntityInfo, RefCounted);

protected:
	static void _bind_methods();

public:
	EntityInfo_t m_entityInfo;

	EntityInfo();
	~EntityInfo();

	bool verify_info();
	void serialize_info();
	void deserialize_info(const unsigned char *data);

	void set_entity_name(String name);
	void set_parent_relative_path(String path);
	void set_entity_id(EntityID_t id);
	void set_associated_player_id(PlayerID_t associatedPlayer);
	String get_entity_name();
	String get_parent_relative_path();
	EntityID_t get_entity_id();
	PlayerID_t get_associated_player_id();
};

//===============GDNet Singleton===============//

class GDNet : public Object {
	GDCLASS(GDNet, Object);

private:
	static ZoneID_t m_zoneIDCounter;

	bool register_network_entities();
	World *get_world_singleton();

protected:
	static void _bind_methods();

public:
	static GDNet *singleton;
	std::map<EntityID_t, NetworkEntityInfo> m_networkEntityRegistry;
	std::map<ZoneID_t, ZoneInfo> m_zoneRegistry;
	World *world;
	bool m_isInitialized;
	bool m_isClient;
	bool m_isServer;

	GDNet();
	~GDNet();
	void cleanup();

	void register_zone(Zone *zone);
	void unregister_zone(Zone *zone);

	void init_gdnet();
	void shutdown_gdnet();

	bool is_client();
	bool is_server();

	bool zone_exists(ZoneID_t zoneId);
	bool entity_exists(EntityID_t entityId);
	EntityID_t get_entity_id_by_name(String entityName);
};

//===============Sync Transform===============//
//class SyncTransform : public Node {
//	GDCLASS(SyncTransform, Node);
//
//protected:
//	static void _bind_methods();
//};

//===============Network Entity===============//

class NetworkEntity : public Node {
	GDCLASS(NetworkEntity, Node);

private:
protected:
	//static void _bind_methods();
	void _notification(int n_type);

public:
};

//===============Zone===============//

class Zone : public Node {
	GDCLASS(Zone, Node);

private:
	Ref<PackedScene> m_zoneScene;

protected:
	static void _bind_methods();
	void _notification(int n_type);

public:
	std::vector<PlayerID_t> m_playersInZone;
	std::map<EntityNetworkID_t, Ref<EntityInfo>> m_entitiesInZone;

	ZoneID_t m_zoneId = 0U;
	bool m_instantiated = false;
	Node *m_zoneInstance = nullptr;

	Zone();
	~Zone();

	Ref<PackedScene> get_zone_scene() const;
	void set_zone_scene(const Ref<PackedScene> &zoneScene);
	ZoneID_t get_zone_id() const;
	void instantiate_zone();
	void uninstantiate_zone();

	void add_player(PlayerID_t playerId);
	bool player_in_zone(PlayerID_t playerId);
	void instantiate_network_entity(EntityID_t entityId, String parentNode);
	void create_entity(Ref<EntityInfo> entityInfo);
};

//===============World===============//

class World : public Object {
	GDCLASS(World, Object);

private:
	std::map<HSteamNetConnection, PlayerInfo> m_worldPlayerInfoByConnection;
	std::map<PlayerID_t, PlayerInfo> m_worldPlayerInfoById;

	//Server Side
	bool m_serverRunLoop;
	HSteamNetPollGroup m_hPollGroup;
	HSteamListenSocket m_hListenSock;
	std::thread m_serverListenThread;

	static void SERVER_SIDE_CONN_CHANGE(SteamNetConnectionStatusChangedCallback_t *pInfo);
	void player_connecting(HSteamNetConnection playerConnection);
	void player_connected(HSteamNetConnection playerConnection);
	void player_disconnected(HSteamNetConnection playerConnection);
	void remove_player(HSteamNetConnection hConn);

	void SERVER_SIDE_load_zone_request(const unsigned char *mssgData, HSteamNetConnection sourceConn);
	void SERVER_SIDE_create_entity_request(const unsigned char *mssgData);
	void SERVER_SIDE_create_entity_acknowledge(const unsigned char *mssgData);

	void SERVER_SIDE_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *pInfo);
	void SERVER_SIDE_poll_incoming_messages();
	void server_listen_loop();

	//Client Side
	bool m_clientRunLoop;
	std::thread m_clientListenThread;

	static void CLIENT_SIDE_CONN_CHANGE(SteamNetConnectionStatusChangedCallback_t *pInfo);

	void CLIENT_SIDE_player_entered_zone(const unsigned char *mssgData);
	void CLIENT_SIDE_load_zone_request(const unsigned char *mssgData);
	void CLIENT_SIDE_assign_player_id(const unsigned char *mssgData);
	void CLIENT_SIDE_process_create_entity_request(const unsigned char *mssgData);

	void CLIENT_SIDE_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *pInfo);
	void CLIENT_SIDE_poll_incoming_messages();
	void client_listen_loop();

protected:
	static void _bind_methods();

public:
	World();
	~World();
	void cleanup();

	bool instantiate_zone_by_id(ZoneID_t zoneId);

	//Server side
	void start_world(int port);
	void stop_world();

	//Client side
	HSteamNetConnection m_worldConnection;

	void join_world(String world, int port);
	void leave_world();

	bool load_zone_by_name(String zoneName);
	bool load_zone_by_id(ZoneID_t zoneId);
	// void unload_zone();

	//Both
	bool player_exists(PlayerID_t playerId);
};

#endif

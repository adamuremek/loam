#ifndef GDNET_H
#define GDNET_H

#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "core/templates/vector.h"
#include "core/math/transform_2d.h"
#include "core/math/transform_3d.h"
#include "core/variant/variant.h"
#include "include/steam/isteamnetworkingutils.h"
#include "include/steam/steamnetworkingsockets.h"
#include "scene/main/node.h"
#include "scene/3d/node_3d.h"
#include "scene/2d/node_2d.h"
#include "scene/resources/packed_scene.h"
#include <algorithm>
#include <cstdint>
#include <stack>
#include <thread>
#include <mutex>
#include <unordered_set>

//===============Data and Types===============//
//Control Message Types (these are events that are fired across the network)
#define ASSIGN_PLAYER_ID static_cast<unsigned char>(0x01)

#define LOAD_ZONE_REQUEST static_cast<unsigned char>(0x02)
#define LOAD_ZONE_DENY static_cast<unsigned char>(0x03)
#define LOAD_ZONE_ACKNOWLEDGE static_cast<unsigned char>(0x04)
#define LOAD_ZONE_COMPLETE static_cast<unsigned char>(0x05)
#define PLAYER_LEFT_ZONE static_cast<unsigned char>(0x06)

#define CREATE_ZONE_PLAYER_INFO_REQUEST static_cast<unsigned char>(0x07)
#define CREATE_ZONE_PLAYER_INFO_ACKNOWLEDGE static_cast<unsigned char>(0x08)

#define CREATE_ENTITY_REQUEST static_cast<unsigned char>(0x10)
#define CREATE_ENTITY_DENY static_cast<unsigned char>(0x11)
#define CREATE_ENTITY_ACKNOWLEDGE static_cast<unsigned char>(0x12)
#define CREATE_ENTITY_COMPLETE static_cast<unsigned char>(0x13)

#define NETWORK_ENTITY_UPDATE static_cast<unsigned char>(0x30)
#define TRANSFORM3D_SYNC_UPDATE static_cast<unsigned char>(0x31)
#define TRANSFORM2D_SYNC_UPDATE static_cast<unsigned char>(0x32)

using PlayerID_t = uint32_t;
using EntityNetworkID_t = uint32_t ;
using EntityID_t = uint32_t;
using ZoneID_t = uint32_t;
using PropertyID_t = uint32_t;
using MessageType_t = uint8_t ;

//Forward declarations
class Zone;
class World;
class EntityInfo;
class NetworkEntity;
class Transform3DSync;
class Transform2DSync;
struct ZoneInfo_t;

//Enum Declarations
enum SyncAuthority{
	NONE,
	OWNER_AUTHORITATIVE,
	SERVER_AUTHORITATIVE
};

//Enum registrations
VARIANT_ENUM_CAST(SyncAuthority)

//This struct is used for server side data storage only
struct PlayerInfo_t {
	PlayerID_t id;
	Zone* currentLoadedZone;
	HashMap<EntityNetworkID_t, Ref<EntityInfo>> ownedEntities;
	//Server-side relavent info only
	HSteamNetConnection playerConnection; // Connection Handle
	bool loadedPlayersInZone;
	bool loadedEntitiesInZone;
	List<EntityNetworkID_t> entitiesWaitingForLoadAck;
	List<PlayerID_t> playersWaitingForLoadAck;
};

struct NetworkEntityInfo_t {
	EntityID_t id;
	String name;
	Ref<PackedScene> scene;
};

struct ZoneInfo_t {
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
	PlayerID_t owner;
	Vector3 initialPosition3D;
	Vector2 initialPosition2D;
	Vector<unsigned char> dataBuffer;

	NetworkEntity *entityInstance;
};

struct EntityUpdateInfo_t{
	ZoneID_t parentZone;
	EntityNetworkID_t networkId;
	unsigned char updateType;
	Vector<unsigned char> dataBuffer;
};


//===============Messaging===============//
SteamNetworkingMessage_t *allocate_message(const unsigned char *data, const int sizeOfData, const HSteamNetConnection &destination);
SteamNetworkingMessage_t *create_mini_message(MessageType_t messageType, unsigned int value, const HSteamNetConnection &destination);
SteamNetworkingMessage_t *create_small_message(MessageType_t messageType, unsigned int value1, unsigned int value2, const HSteamNetConnection &destination);
//SteamNetworkingMessage_t *instantiate_entity_message(const EntityID_t entityID, String parentNode, const HSteamNetConnection &destination);

void serialize_int(int value, int startIdx, Vector<unsigned char> &buffer);
void serialize_uint(unsigned int value, int startIdx, Vector<unsigned char> &buffer);
//TODO: Implement - void serialize_string();
void serialize_vector3(const Vector3 &vec, int startIdx, Vector<unsigned char> &buffer);
void serialize_vector2(const Vector2 &vec, int startIdx, Vector<unsigned char> &buffer);
void serialize_transform3d(const Transform3D &transform, int startIdx, Vector<unsigned char> &buffer);
void serialize_transform2d(const Transform2D &transform, int startIdx, Vector<unsigned char> &buffer);

template<typename T>
void serialize_basic(const T &value, int startIdx, Vector<unsigned char> &buffer){
	memcpy(buffer.ptrw() + startIdx, &value, sizeof(T));
}

template<typename T>
T deserialize_basic(int startIdx, const unsigned char *buffer){
	T incomingVal;

	//Copy data from buffer into the type
	memcpy(&incomingVal, buffer + startIdx, sizeof(T));

	return incomingVal;
}

int deserialize_int(int startIdx, const unsigned char *buffer);
unsigned int deserialize_uint(int startIdx, const unsigned char *buffer);
String deserialize_string(int startIdx, int stringLength, const unsigned char *buffer);
Vector3 deserialize_vector3(int startIdx, const unsigned char *buffer);
Transform3D deserialize_transform3d(int startIdx, const unsigned char *buffer);
//TODO: Either implement or remove - void copy_string_to_buffer(const char *string, unsigned char *buffer, int startIDx, int stringSize);
unsigned int deserialize_mini(const unsigned char *data);
void deserialize_small(const unsigned char *data, unsigned int &value1, unsigned int &value2);

void send_message_reliable(SteamNetworkingMessage_t *message);
void send_message_unreliable(SteamNetworkingMessage_t *message);

//===============GDNet Debug===============//

//class GDNetDebug : public Object{
//	GDCLASS(GDNetDebug, Object);
//};

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
	HashMap<EntityID_t, NetworkEntityInfo_t> m_networkEntityRegistry;
	HashMap<ZoneID_t, ZoneInfo_t> m_zoneRegistry;
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

//===============Player Info===============//
class PlayerInfo : public RefCounted{
	GDCLASS(PlayerInfo, RefCounted);

protected:
	static void _bind_methods();

public:
	PlayerInfo_t m_playerInfo;

	PlayerInfo();
	~PlayerInfo();

	void load_player(Ref<PlayerInfo> playerInfo);
	void load_entity(Ref<EntityInfo> entityInfo);
	void load_entities_in_current_zone();
	void add_owned_entity(Ref<EntityInfo> associatedEntity);
	void confirm_player_load(PlayerID_t playerId);
	void confirm_entity_load(EntityNetworkID_t entityNetworkId);
	void reset_zone_info();

	PlayerID_t get_player_id();
	HSteamNetConnection get_player_conn();
	Zone* get_current_loaded_zone();

	void set_player_id(PlayerID_t playerId);
	void set_player_conn(HSteamNetConnection playerConnection);
	void set_current_loaded_zone(Zone* zone);
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

	//void add_();

	String get_entity_name();
	String get_parent_relative_path();
	EntityID_t get_entity_id();
	PlayerID_t get_owner_id();
	EntityNetworkID_t get_network_id();
	Vector3 get_initial_position_3D();
	Vector2 get_initial_position_2D();

	void set_entity_name(String name);
	void set_parent_relative_path(String path);
	void set_entity_id(EntityID_t id);
	void set_owner_id(PlayerID_t ownerId);
	void set_network_id(EntityNetworkID_t networkId);
	void set_initial_position_3D(Vector3 pos);
	void set_initial_position_2D(Vector2 pos);
};

//===============Network Entity===============//

class NetworkEntity : public Node {
	GDCLASS(NetworkEntity, Node);

private:
	Ref<Transform3DSync> m_transform3DSync;
	Ref<Transform2DSync> m_transform2DSync;

	void _ready();
	void _process(float delta);

protected:
	static void _bind_methods();
	void _notification(int n_type);

public:
	Ref<EntityInfo> m_info;
	Zone* m_parentZone;

	NetworkEntity();

	bool has_ownership();
	void SERVER_SIDE_tick();
	void SERVER_SIDE_recieve_data(EntityUpdateInfo_t updateInfo);
	void SERVER_SIDE_transmit_data();
	void CLINET_SIDE_tick();
	void CLIENT_SIDE_recieve_data(EntityUpdateInfo_t updateInfo);
	void CLIENT_SIDE_transmit_data();

	Ref<Transform3DSync> get_transform3d_sync();
	Ref<Transform2DSync> get_transform2d_sync();
	Ref<EntityInfo> get_entity_info();

	void set_transform3d_sync(Ref<Transform3DSync> transform3DSync);
	void set_transform2d_sync(Ref<Transform2DSync> transform2DSync);

};

//===============Network Module===============//

class NetworkModule : public RefCounted{
	GDCLASS(NetworkModule, RefCounted);

private:
	virtual void serialize_payload(EntityUpdateInfo_t &updateInfo);
	virtual void deserialize_payload(const EntityUpdateInfo_t &updateInfo);
protected:
	int m_tickCount = 0;

	//Transmission rate in HZ
	int m_transmissionRate = 0;
	//Default transmission rate is 20hz.
	int m_maxTickCount = 50;

	static void _bind_methods();
public:
	static const int METADATA_SIZE;
	NetworkEntity *m_parentNetworkEntity = nullptr;

	virtual void tick();
	virtual void transmit_data(HSteamNetConnection destination);
	virtual void recieve_data(EntityUpdateInfo_t updateInfo);
	static void serialize_update_metadata(EntityUpdateInfo_t &updateInfo);
	static EntityUpdateInfo_t deserialize_update_metadata(const unsigned char* mssgData, const int mssgLen);

	int get_transmission_rate() const;

	void set_transmission_rate(const int &transmissionRate);
};

//===============Transform 3D Sync===============//
class Transform3DSync : public NetworkModule {
	GDCLASS(Transform3DSync, NetworkModule);

private:
	Transform3D global_transform;
	Node3D* m_target;
	SyncAuthority m_authority;

	void serialize_payload(EntityUpdateInfo_t &updateInfo) override;
	void deserialize_payload(const EntityUpdateInfo_t &updateInfo) override;
protected:
	static void _bind_methods();

public:
	Vector3 m_targetPosition;
	Vector3 m_currentPosition;
	float m_interpolationTime;
	float m_elapsedTime;

	Transform3DSync();

	void transmit_data(HSteamNetConnection destination) override;
	void recieve_data(EntityUpdateInfo_t updateInfo) override;
	void interpolate_origin(float delta);
	bool has_authority();
	bool has_target();
	void update_transform_data();

	Node3D* get_target();
	Vector3 get_position() const;
	SyncAuthority get_authority() const;

	void set_target(Node3D* target);
	void set_position(const Vector3 &position);
	void set_authority(SyncAuthority authority);
};

//===============Transform 2D Sync===============//
class Transform2DSync : public NetworkModule {
	GDCLASS(Transform2DSync, NetworkModule);

private:
	bool m_interpolate;
	Transform2D global_transform;
	Node2D* m_target;
	SyncAuthority m_authority;

	void serialize_payload(EntityUpdateInfo_t &updateInfo) override;
	void deserialize_payload(const EntityUpdateInfo_t &updateInfo) override;
protected:
	static void _bind_methods();

public:
	Vector2 m_targetPosition;
	Vector2 m_currentPosition;
	float m_interpolationTime;
	float m_elapsedTime;

	Transform2DSync();

	void tick() override;
	void transmit_data(HSteamNetConnection destination) override;
	void recieve_data(EntityUpdateInfo_t updateInfo) override;
	void interpolate_origin(float delta);
	bool has_authority();
	bool has_target();
	void update_transform_data();
	void copy_transform();

	bool get_interpolate() const;
	Node2D* get_target() ;
	Vector2 get_position() const;
	SyncAuthority get_authority() const;

	void set_interpolate(const bool &interpolate);
	void set_target(Node2D* target);
	void set_position(const Vector2 &position);
	void set_authority(SyncAuthority authority);
};

//===============Zone===============//

class Zone : public Node {
	GDCLASS(Zone, Node);

private:
	Ref<PackedScene> m_zoneScene;
	ZoneID_t m_zoneId;
	bool m_instantiated;
	Node *m_zoneInstance;

protected:
	static void _bind_methods();
	void _notification(int n_type);

public:
	HashMap<PlayerID_t, Ref<PlayerInfo>> m_playersInZone;
	HashMap<EntityNetworkID_t, Ref<EntityInfo>> m_entitiesInZone;

	Zone();
	~Zone();

	bool instantiate_zone();
	void uninstantiate_zone();
	void add_player(Ref<PlayerInfo> playerInfo);
	void remove_player(Ref<PlayerInfo> playerInfo);
	void load_entity(Ref<EntityInfo> entityInfo);
	void create_entity(Ref<EntityInfo> entityInfo);
	void destroy_entity(Ref<EntityInfo> entityInfo);

	void player_loaded_callback(Ref<PlayerInfo> playerInfo);

	bool player_in_zone(PlayerID_t player);
	bool is_instantiated();

	Ref<PackedScene> get_zone_scene() const;
	Ref<PlayerInfo> get_player(PlayerID_t playerId) const;
	ZoneID_t get_zone_id() const;

	void set_zone_scene(const Ref<PackedScene> &zoneScene);
	void set_zone_id(const ZoneID_t zoneId);
};

//===============World===============//

class World : public Object {
	GDCLASS(World, Object);

private:
	HashMap<HSteamNetConnection, Ref<PlayerInfo>> m_worldPlayerInfoByConnection;
	HashMap<PlayerID_t, Ref<PlayerInfo>> m_worldPlayerInfoById;

	//Server Side
	bool m_serverRunLoop;
	HSteamNetPollGroup m_hPollGroup;
	HSteamListenSocket m_hListenSock;
	std::thread m_serverListenThread;
	std::thread m_serverTickThread;

	static void SERVER_SIDE_CONN_CHANGE(SteamNetConnectionStatusChangedCallback_t *pInfo);
	void player_connecting(HSteamNetConnection playerConnection);
	void player_connected(HSteamNetConnection playerConnection);
	void player_disconnected(HSteamNetConnection playerConnection);
	void remove_player(HSteamNetConnection hConn);

	void SERVER_SIDE_load_zone_request(const unsigned char *mssgData, HSteamNetConnection sourceConn);
	void SERVER_SIDE_load_zone_acknowledge(const unsigned char *mssgData, HSteamNetConnection sourceConn);
	void SERVER_SIDE_create_zone_player_info_acknowledge(const unsigned char *mssgData, HSteamNetConnection sourceConn);
	void SERVER_SIDE_load_entity_request(const unsigned char *mssgData);
	void SERVER_SIDE_load_entity_acknowledge(const unsigned char *mssgData, HSteamNetConnection sourceConn);
	void SERVER_SIDE_handle_entity_update(const unsigned char *mssgData, const int mssgLen);
	void SERVER_SIDE_player_left_zone(const unsigned char *mssgData);

	void SERVER_SIDE_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *pInfo);
	void SERVER_SIDE_poll_incoming_messages();
	void server_listen_loop();
	void server_tick_loop();

	//Client Side
	Ref<PlayerInfo> m_localPlayer;
	bool m_clientRunLoop;
	std::thread m_clientListenThread;
	std::thread m_clientTickThread;

	static void CLIENT_SIDE_CONN_CHANGE(SteamNetConnectionStatusChangedCallback_t *pInfo);

	void CLIENT_SIDE_assign_player_id(const unsigned char *mssgData);
	void CLIENT_SIDE_zone_load_complete(const unsigned char *mssgData);
	void CLIENT_SIDE_load_zone_request(const unsigned char *mssgData);
	void CLIENT_SIDE_process_create_zone_player_info_request(const unsigned char *mssgData);
	void CLIENT_SIDE_load_entity_request(const unsigned char *mssgData);
	void CLIENT_SIDE_handle_entity_update(const unsigned char *mssgData, const int mssgLen);
	void CLIENT_SIDE_player_left_zone(const unsigned char *mssgData);

	void CLIENT_SIDE_connection_status_changed(SteamNetConnectionStatusChangedCallback_t *pInfo);
	void CLIENT_SIDE_poll_incoming_messages();
	void client_listen_loop();
	void client_tick_loop();

protected:
	static void _bind_methods();

public:
	World();
	~World();
	void cleanup();



	//Server side
	void start_world(int port);
	void stop_world();

	//Client side
	HSteamNetConnection m_worldConnection;

	bool CLIENT_SIDE_instantiate_zone(ZoneID_t zoneId);

	PlayerID_t get_player_id();
	void join_world(String world, int port);
	void leave_world();

	bool load_zone_by_name(String zoneName);
	bool load_zone_by_id(ZoneID_t zoneId);
	void unload_zone();

	//Both
	bool player_exists(PlayerID_t playerId);
};

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

#endif

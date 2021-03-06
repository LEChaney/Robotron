#include "NetworkServerSystem.h"

#include "socket.h"
#include "PlayerInfo.h"
#include "Scene.h"
#include "Packet.h"
#include "Entity.h"
#include "Utils.h"
#include "ClientInfo.h"
#include "GhostSnapshot.h"
#include "NetworkEventListener.h"

#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include "Log.h"
#include <chrono>

using namespace std::chrono;
using namespace std::chrono_literals;

const USHORT NetworkServerSystem::s_kDefaultServerPort = 8456;

NetworkServerSystem::NetworkServerSystem(Scene& scene, const std::string& username, const std::string& serverName)
	: NetworkSystem(scene)
	, m_serverState{ SERVER_STATE_LOBBY_MODE }
	, m_serverName{ serverName }
	, m_willSpawnPlayersThisFrame{ false }
	, m_firstFrame {true}
{
	m_scene.registerEntityEventListener(this);

	m_socket.initialise(s_kDefaultServerPort);
	allocateRecvBuffer();
	RPC::setServer(this);

	m_serverPlayerInfo.username = username;
	m_serverPlayerInfo.setUniquePlayerID();

	// TODO: Create client connection code instead of hard coding single
	// client
	// TEMP hard coded client (localhost:4567)
	//sockaddr_in address;
	//inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
	//address.sin_port = htons(4567);
	//address.sin_family = AF_INET;
	//ClientInfo clientInfo;
	//clientInfo.lastSeqNumSeen = 0;
	//clientInfo.playerInfo.username = "Default";
	//clientInfo.playerInfo.lives = 3;
	//clientInfo.playerInfo.score = 0;
	//m_clients.insert(std::make_pair(address, clientInfo));

	// Temp socket for receiving from self for testing.
	//m_socket2.initialise(4567);
}

NetworkServerSystem::~NetworkServerSystem()
{
	m_scene.removeEntityEventListener(this);
}

void NetworkServerSystem::spawnPlayers()
{
	// Create an entity for the server to possess
	TransformComponent transform{};
	transform.position.y = 1;
	m_serverPlayer = &EntityUtils::createPlayer(m_scene, transform);
	m_serverPlayer->player.playerInfo = m_serverPlayerInfo;

	// Give the local player a spotlight
	m_serverPlayer->addComponents(COMPONENT_SPOTLIGHT);
	m_serverPlayer->spotlight.direction = glm::vec3(0, -0.1, -1);
	m_serverPlayer->spotlight.color = glm::vec3(0.5, 0.75, 1.5) * 4.0f;

	// Tell clients to create the server player model
	bufferRpc(std::make_unique<RPCCreatePlayerGhost>(m_serverPlayer->network.id, m_serverPlayer->player.playerInfo));

	// Create entities for clients to possess
	for (auto& addressClientInfoPair : m_clients) {
		Entity& newPlayer = EntityUtils::createPlayer(m_scene, transform);
		newPlayer.removeComponents(COMPONENT_INPUT_MAP); // Input will come from the clients
		newPlayer.network.clientAddress = addressClientInfoPair.first;
		newPlayer.player.playerInfo = addressClientInfoPair.second.playerInfo;
		addressClientInfoPair.second.playerEntity = &newPlayer;

		// Tell clients to create their player models
		bufferRpc(std::make_unique<RPCCreatePlayerGhost>(newPlayer.network.id, newPlayer.player.playerInfo));
	}
}

void NetworkServerSystem::handleGamePackets(const Packet& packet, const sockaddr_in& address)
{
	if (packet.packetType != PACKET_TYPE_NORMAL) {
		// TODO: Add logging here
		g_out << "WARNING: Received lobby packet while in game\n";
		return;
	}

	// Check we have a client associated with this address
	auto clientIt = m_clients.find(address);
	if (clientIt == m_clients.end()) {
		// TODO: Add logging here
		g_out << "INFO: Received a packet from an unassociated client" 
		          << "\n";
		return;
	}

	// Get figure out which remote procedure calls we got from the client
	// that we haven't seen before.
	std::uint32_t seqNumSeen = clientIt->second.lastSeqNumSeen;
	std::uint32_t seqNumRecvd = packet.sequenceNum;
	std::int32_t bufferOffset = static_cast<std::int32_t>(seqNumRecvd - seqNumSeen - 1);
	if (bufferOffset > packet.rpcGroupBuffer.size() - 1) {
		g_out << "WARNING: Missed packet count exceeds redundancy buffer size, may miss Remote Procedure Calls\n";
		bufferOffset = static_cast<std::int32_t>(packet.rpcGroupBuffer.size()) - 1;
	}
		
	// Execute RPCs received from client
	for (std::int32_t i = bufferOffset; i >= 0; --i) {
		const RPCGroup& rpcGroup = packet.rpcGroupBuffer.at(i);
		for (auto& rpc : rpcGroup.getRpcs()) {
			// TODO: Add check to make sure rpc is being executed
			// on an entity that the client actually controls (security)
			rpc->execute();
		}
	}

	// Update the last sequence number seen from the client
	// TODO: Handle out of order packets similar to client side
	if (seqNumRecvd > seqNumSeen)
		clientIt->second.lastSeqNumSeen = seqNumRecvd;

	// Update keep alive
	clientIt->second.tLastPacketRecvd = high_resolution_clock::now();
}

void NetworkServerSystem::handleLobbyPackets(const Packet& packet, const sockaddr_in& address)
{
	switch (packet.packetType)
	{
	case PACKET_TYPE_NORMAL:
		handleGamePackets(packet, address);
		break;
	case PACKET_TYPE_BROADCAST:
		handleBroadcastPacket(packet, address);
		break;
	case PACKET_TYPE_JOIN_REQUEST:
		handleJoinPacket(packet, address);
		break;
	default:
		break;
	}
}

void NetworkServerSystem::handleBroadcastPacket(const Packet& packet, const sockaddr_in& address)
{
	// TODO: Add logging here
	g_out << "INFO: Server received a broadcast packet from address: " 
	          << toString(address) << "\n";

	Packet broadcastResp;
	broadcastResp.packetType = PACKET_TYPE_BROADCAST_RESPONSE;
	// TODO: Add server name from UI input
	broadcastResp.serverName = m_serverName;
	sendData(broadcastResp, address);
}

void NetworkServerSystem::handleJoinPacket(const Packet& packet, const sockaddr_in& address)
{
	// TODO: Add logging here
	g_out << "INFO: Server received a join request from user: " 
	          << packet.username << ", at address: " << toString(address) 
	          << "\n";

	ClientInfo client;
	client.playerInfo.setUniquePlayerID();
	client.playerInfo.username = packet.username;
	client.tLastPacketRecvd = high_resolution_clock::now();

	m_clients.insert(std::make_pair(address, client));

	Packet joinResp;
	joinResp.packetType = PACKET_TYPE_JOIN_RESPONSE;
	joinResp.playerID = client.playerInfo.getPlayerID();
	joinResp.joinAccepted = true; // Just always accept join requests for now
	sendData(joinResp, address);

	// Send RPCs to clients informing them of lobby updates
	std::vector<PlayerInfo> currentPlayers = getPlayers();
	bufferRpc(std::make_unique<RPCUpdatePlayers>(currentPlayers));
	
	if (m_serverState == SERVER_STATE_LOBBY_MODE && m_eventListeners.size() > 0)
	{
		for (auto eventListener : m_eventListeners)
			eventListener->onPlayersUpdated(getPlayers());
	}
	// TODO: Don't auto start the game, stay in lobby until game is started
	//startGame();
}

std::vector<PlayerInfo> NetworkServerSystem::getPlayers()
{
	// TODO: Add server player info as well
	std::vector<PlayerInfo> playerInfo;
	playerInfo.push_back(m_serverPlayerInfo);
	for (auto& client : m_clients) {
		playerInfo.push_back(client.second.playerInfo);
	}
	return playerInfo;
}

void NetworkServerSystem::beginFrame()
{
	// Inform each network entity that it has not yet received
	// input this frame.
	for (Entity* entity : m_netEntities) {
		if (entity && entity->hasComponents(COMPONENT_INPUT)) {
			entity->network.receivedInputThisFrame = false;
		}
	}

	sockaddr_in address;
	while (receiveData(m_recvPacket, address)) {
		// Receive and process packets
		switch (m_serverState)
		{
		case SERVER_STATE_LOBBY_MODE:
			handleLobbyPackets(m_recvPacket, address);
			break;
		case SERVER_STATE_IN_GAME:
			handleGamePackets(m_recvPacket, address);
			break;
		default:
			break;
		}
	}

	// Check keep alives
	auto now = high_resolution_clock::now();
	auto clientIt = m_clients.begin();
	bool clientDisconnected = false;
	while (clientIt != m_clients.end()) {
		auto tLastPacketRecvd = clientIt->second.tLastPacketRecvd;
		auto timeSinceLastPacket = now - tLastPacketRecvd;
		if (timeSinceLastPacket > NetworkSystem::s_kKeepAliveTimout) {
			if (clientIt->second.playerEntity)
				m_scene.destroyEntity(*clientIt->second.playerEntity);
			clientIt = m_clients.erase(clientIt);
			clientDisconnected = true;
		} else {
			++clientIt;
		}
	}

	// Notify UI of any client disconnections
	if (clientDisconnected && m_eventListeners.size() > 0) {
		std::vector<PlayerInfo> players = getPlayers();
		for (auto eventListener : m_eventListeners)
			eventListener->onPlayersUpdated(players);
	}
	
	// Check if we will send packets this frame
	NetworkSystem::beginFrame();

	// Don't send packets if we have noone to send to
	if (m_clients.size() == 0) {
		m_willSendPcktThisFrame = false;
	}
}

void NetworkServerSystem::update(Entity& entity, float deltaTick)
{
	if (m_serverState != SERVER_STATE_IN_GAME)
		return;

	std::int32_t id = entity.network.id;

	// Filter out non-networked entities
	if (!entity.hasComponents(COMPONENT_NETWORK))
		return;

	// Fast forward input state to the last input received
	// if we didn't receive input this frame.
	if (entity.hasComponents(COMPONENT_INPUT) 
		&& !entity.network.receivedInputThisFrame) {
		entity.input = entity.network.lastInputReceived;
	}

	// Notify clients of updates to player lives and score
	if (entity.hasComponents(COMPONENT_PLAYER) && entity.player.playerInfo.hasChanged()) {
		PlayerInfo& playerInfo = entity.player.playerInfo;
		
		if (playerInfo.getPlayerID() == m_serverPlayerInfo.getPlayerID()) {
			// Update server player-info on change
			m_serverPlayerInfo = playerInfo;
		} else {
			// Update client player-info on change
			m_clients.at(entity.network.clientAddress).playerInfo = playerInfo;
		}

		// Trigger events
		std::vector<PlayerInfo> currentPlayers = getPlayers();
		bufferRpc(std::make_unique<RPCUpdatePlayers>(currentPlayers));

		if (m_eventListeners.size() > 0)
		{
			for (auto eventListener : m_eventListeners)
				eventListener->onPlayersUpdated(currentPlayers);
		}
	}
}

void NetworkServerSystem::endFrame()
{
	// Spawn players if we need to
	if (m_willSpawnPlayersThisFrame) {
		spawnPlayers();
		m_willSpawnPlayersThisFrame = false;
	}

	if (m_willSendPcktThisFrame) {
		// Set packet sequence number
		m_sendPacket.sequenceNum = m_curSeqenceNum;

		selectGhostSnapshots(m_sendPacket.ghostSnapshotBuffer, m_netEntities,
		                     m_sendPacket.ghostSnapshotBuffer.getMaxSize());

		// Send packet to clients
		m_sendPacket.packetType = PACKET_TYPE_NORMAL;
		broadcastToClients(m_sendPacket);
	}

	if (m_firstFrame){
		for (auto eventListener : m_eventListeners)
			eventListener->onPlayersUpdated(getPlayers());
		m_firstFrame = false;
	}

	NetworkSystem::endFrame();
}

bool NetworkServerSystem::isInGame()
{
	return m_serverState == SERVER_STATE_IN_GAME;
}

void NetworkServerSystem::startGame()
{
	m_serverState = SERVER_STATE_IN_GAME;

	// Tell the clients to start their game instance
	bufferRpc(std::make_unique<RPCStartGame>());

	m_willSpawnPlayersThisFrame = true;
}

void NetworkServerSystem::recordInput(std::int32_t entityNetId, const InputComponent& input)
{
	if (entityNetId < m_netEntities.size()) {
		Entity* entity = m_netEntities.at(entityNetId);
		if (entity) {
			// Set the local input state in a such a way that button
			// states updated from the network remain down for at least one
			// frame so they can be processed by other systems.
			entity->input.axis = input.axis;
			entity->input.orientationDelta = input.orientationDelta;
			entity->input.btn1Down       = entity->input.btn1Down       || input.btn1Down;
			entity->input.btn2Down       = entity->input.btn2Down       || input.btn2Down;
			entity->input.btn3Down       = entity->input.btn3Down       || input.btn3Down;
			entity->input.btn4Down       = entity->input.btn4Down       || input.btn4Down;
			entity->input.shootLeftDown  = entity->input.shootLeftDown  || input.shootLeftDown ;
			entity->input.shootDown      = entity->input.shootDown      || input.shootDown     ;
			entity->input.shootRightDown = entity->input.shootRightDown || input.shootRightDown;
			entity->input.shootLeft      = entity->input.shootLeft      || input.shootLeft     ;
			entity->input.shootRight     = entity->input.shootRight     || input.shootRight    ;
			entity->input.shootLeftUp    = entity->input.shootLeftUp    || input.shootLeftUp   ;
			entity->input.shootUp        = entity->input.shootUp        || input.shootUp       ;
			entity->input.shootRightUp   = entity->input.shootRightUp   || input.shootRightUp  ;

			// These are used to set the input to the last received input
			// after one frame.
			entity->network.lastInputReceived = input;
			entity->network.receivedInputThisFrame = true;
		}
		else {
			// TODO: Add logging here
			g_out << "Info: Received input for a destroyed entity\n";
		}
	}
	else {
		// TODO: Add logging here
		g_out << "ERROR: Received input for an entity with an out of range network id\n";
	}
}

void NetworkServerSystem::sendAudio(Sound sound)
{
	bufferRpc(std::make_unique<RPCPlayAudio>(sound));
}

void NetworkServerSystem::broadcastToClients(const Packet& packet)
{
	for (auto& addressClientInfoPair : m_clients) {
		sendData(packet, addressClientInfoPair.first);
	}
}

void NetworkServerSystem::selectGhostSnapshots(SnapshotBufT& dst, 
                                               std::vector<Entity*>& src,
                                               size_t maxSnapshots)
{
	using PriorityQT = std::priority_queue<Entity*, std::vector<Entity*>, EntityPriorityComparitor>;
	static PriorityQT s_snapshotPriorityQ;

	// Create priority queue of entities from src
	s_snapshotPriorityQ = {};
	for (Entity* entity : src) {
		if (entity) {
			++entity->network.priority;
			s_snapshotPriorityQ.push(entity);
		}
	}

	// Clear destination snapshot buffer
	dst.clear();

	// Pull snapshots from priority queue
	for (size_t i = 0; i < maxSnapshots && !s_snapshotPriorityQ.empty(); ++i) {
		Entity* entity = s_snapshotPriorityQ.top();
		s_snapshotPriorityQ.pop();
		dst.emplace_back(entity->network.id, entity->transform, entity->physics);
		entity->network.priority = 0;
	}
}

void NetworkServerSystem::onEntityCreation(Entity & entity)
{

}

void NetworkServerSystem::onEntityDestruction(Entity& entity)
{
	
}

void NetworkServerSystem::onAddComponents(Entity& entity, size_t componentMaskAdded)
{
	if (!Entity::matches(componentMaskAdded, COMPONENT_NETWORK))
		return;

	std::int32_t id = entity.network.id;

	// Check if we can reuse the entities current id
	if (0 <= id && id < m_netEntities.size() && !m_netEntities.at(id)) {
		// Reuse existing id from old entity
		m_netEntities.at(id) = &entity;
	}
	else {
		// Check for a free id
		auto freeIt = std::find(m_netEntities.begin(), m_netEntities.end(), nullptr);
		if (freeIt != m_netEntities.end()) {
			// Assign the new entity a free id if found
			entity.network.id = static_cast<std::int32_t>(std::distance(m_netEntities.begin(), freeIt));
			*freeIt = &entity;
		}
		else {
			// Otherwise assign the new entity a brand new id
			entity.network.id = static_cast<std::int32_t>(m_netEntities.size());
			m_netEntities.push_back(&entity);
		}
	}

	// Create remote procedure calls to inform clients of new entity creation
	id = entity.network.id;
	std::unique_ptr<RemoteProcedureCall> rpc;
	// TODO: Remove redundant transform
	if (entity.hasComponents(COMPONENT_PLAYER)) {
		// Delay until we have player info
		return;
	}
	else if (entity.hasComponents(COMPONENT_ZOMBIE)) {
		rpc = std::make_unique<RPCCreateGhost>(id, ModelID::MODEL_ENEMY_ZOMBIE);
	}
	else if (entity.hasComponents(COMPONENT_SNAKE)) {
		rpc = std::make_unique<RPCCreateGhost>(id, ModelID::MODEL_ENEMY_SNAKE);
	}
	else if (entity.hasComponents(COMPONENT_ENEMY_SHOOTER)) {
		rpc = std::make_unique<RPCCreateGhost>(id, ModelID::MODEL_ENEMY_SHOOTER);
	}
	else if (entity.hasComponents(COMPONENT_PLAYERBULLET)) {
		rpc = std::make_unique <RPCCreateGhost>(id, ModelID::MODEL_PLAYER_BULLET);
	}
	else if (entity.hasComponents(COMPONENT_ENEMYBULLET)) {
		rpc = std::make_unique <RPCCreateGhost>(id, ModelID::MODEL_ENEMY_BULLET);
	}
	else if (entity.hasComponents(COMPONENT_SCOREPICKUP)) {
		if (entity.aiVariables.lifePickUp) {
			rpc = std::make_unique <RPCCreateGhost>(id, ModelID::MODEL_HEALTH_PICKUP);
		}
		else if (entity.aiVariables.score > 10) {
			rpc = std::make_unique <RPCCreateGhost>(id, ModelID::MODEL_SCORE_PICKUP_2);
		}
		else {
			rpc = std::make_unique <RPCCreateGhost>(id, ModelID::MODEL_SCORE_PICKUP_1);
		}
	}
	else {
		rpc = nullptr;
		g_out << "ERROR: Server received entity creation event for unknown entity type. Unable to update clients\n";
	}

	if (rpc) {
		bufferRpc(std::move(rpc));
	}
}

void NetworkServerSystem::onRemoveComponents(Entity& entity, size_t componentMaskToRemove)
{
	if (!Entity::matches(componentMaskToRemove, COMPONENT_NETWORK))
		return;

	// Notify clients of entity destruction
	std::int32_t id = entity.network.id;
	auto rpc = std::make_unique<RPCDestroyGhost>(id);
	bufferRpc(std::move(rpc));

	// Stop tracking entity on the server
	m_netEntities.at(id) = nullptr;
}

#include "MessageIdentifiers.h"
#include "RakPeerInterface.h"
#include "BitStream.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <map>
#include <mutex>

static unsigned int SERVER_PORT = 65000;
static unsigned int CLIENT_PORT = 65001;
static unsigned int MAX_CONNECTIONS = 4;


enum NetworkState
{
	NS_Init = 0,
	NS_PendingStart,
	NS_Started,
	NS_Lobby,
	NS_Pending,
	NS_ReadyForGame,

};

bool isServer = false;
bool isRunning = true;

RakNet::RakPeerInterface *g_rakPeerInterface = nullptr;
RakNet::SystemAddress g_serverAddress;

std::mutex g_networkState_mutex;
NetworkState g_networkState = NS_Init;

enum {
ID_THEGAME_LOBBY_READY = ID_USER_PACKET_ENUM,
ID_PLAYER_READY,
ID_THEGAME_START, 
ID_PLAYER_CLASS,
ID_DISPLAY_STATUS,
 };

enum EPlayerClass
{
	Warrior=1,
	Rogue,
	Cleric,
};



struct SPlayer
{
	std::string m_name;
	
	EPlayerClass m_class;
	unsigned int m_health;
	unsigned int m_heal;
	unsigned int m_dodge;
	unsigned int m_damage;
	unsigned int m_counter;

	//function to send a packet with name/health/class etc
	// sends to client
	void SendName(RakNet::SystemAddress systemAddress, bool isBroadcast)
	{
		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_PLAYER_READY);
		RakNet::RakString name(m_name.c_str());
		writeBs.Write(name);
		std::cout << "Ddfdf";
		//returns 0 when something is wrong
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, systemAddress, isBroadcast));
	}

	void SendClass(RakNet::SystemAddress systemAddress, bool isBroadcast)
	{

		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_PLAYER_CLASS);

		/*std::string playerClassStr;
		switch (m_class)
		{
		case Warrior:
			playerClassStr = "Warrior";
			break;
		case Rogue:
			playerClassStr = "Rogue";
			break;
		case Cleric:
			playerClassStr = "Cleric";
			break;
		default:
			break;
		}*/
		RakNet::RakString name(m_name.c_str());
		writeBs.Write(name);
		writeBs.Write(m_class);

		//returns 0 when something is wrong
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, systemAddress, isBroadcast));
	}



	void SendStats(RakNet::SystemAddress systemAddress, bool isBroadcast) {
		std::cout << "\n SendStats";
		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_DISPLAY_STATUS);
		/*RakNet::RakString name(m_name.c_str());
		writeBs.Write(name);*/
		writeBs.Write(m_health);
		writeBs.Write(m_damage);
		writeBs.Write(m_heal);

		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, systemAddress, isBroadcast));
	}
};

std::map<unsigned long, SPlayer> m_players;

SPlayer& GetPlayer(RakNet::RakNetGUID raknetId)
{
	unsigned long guid = RakNet::RakNetGUID::ToUint32(raknetId);
	std::map<unsigned long, SPlayer>::iterator it = m_players.find(guid);
	assert(it != m_players.end());
	return it->second;
}

void OnLostConnection(RakNet::Packet* packet)
{
	SPlayer& lostPlayer = GetPlayer(packet->guid);
	lostPlayer.SendName(RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
	unsigned long keyVal = RakNet::RakNetGUID::ToUint32(packet->guid);
	m_players.erase(keyVal);
}

//server
void OnIncomingConnection(RakNet::Packet* packet)
{
	//must be server in order to recieve connection
	assert(isServer);
	m_players.insert(std::make_pair(RakNet::RakNetGUID::ToUint32(packet->guid), SPlayer()));
	std::cout << "Total Players: " << m_players.size() << std::endl;
	
}

//client
void OnConnectionAccepted(RakNet::Packet* packet)
{
	//server should not ne connecting to anybody, 
	//clients connect to server
	assert(!isServer);
	g_networkState_mutex.lock();
	g_networkState = NS_Lobby;
	g_networkState_mutex.unlock();
	g_serverAddress = packet->systemAddress;
}

//this is on the client side
void DisplayPlayerReady(RakNet::Packet* packet)
{

	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);
	
	std::cout << userName.C_String() << "(" << RakNet::RakNetGUID::ToUint32(packet->guid) << ") has joined" << std::endl;
}

void ApplyPlayerClass(RakNet::Packet* packet)
{
	std::cout << RakNet::RakNetGUID::ToUint32(packet->guid) << " APPLY CLASS" << std::endl;
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString pclass;
	bs.Read(pclass);
	
	SPlayer& player = GetPlayer(packet->guid);
	if (pclass == '1') {
		player.m_class = Warrior;
		player.m_health = 100;		
		player.m_damage = 20;
		player.m_heal = 10;
	}
	else if (pclass == '2') {
		player.m_class = Rogue;
		player.m_health = 80;
		player.m_damage = 40;
		player.m_heal = 10;
		
	}
	else if (pclass == '3') {
		player.m_class = Cleric;		
		player.m_health = 80;
		player.m_damage = 20;
		player.m_heal = 40;
	}
	
	std::cout << "Health : " << player.m_health << " Damage : " << player.m_damage << " Heal : " << player.m_heal;
}



void OnLobbyReady(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
	SPlayer& player = GetPlayer(packet->guid);
	player.m_name = userName;
	//std::cout << userName.C_String() << " aka " << player.m_name.c_str() << " IS READY!!!!!" << std::endl;

	//notify all other connected players that this plyer has joined the game
	for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
	{
		//skip over the player who just joined
		if (guid == it->first)
		{
			continue;
		}

		SPlayer& player = it->second;
		player.SendName(packet->systemAddress, false);
		/*RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_PLAYER_READY);
		RakNet::RakString name(player.m_name.c_str());
		writeBs.Write(name);

		//returns 0 when something is wrong
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));*/
	}
	
	player.SendName(packet->systemAddress, true);
	/*RakNet::BitStream writeBs;
	writeBs.Write((RakNet::MessageID)ID_PLAYER_READY);
	RakNet::RakString name(player.m_name.c_str());
	writeBs.Write(name);
	assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true));*/

}

void DisplayStatus(RakNet::Packet* packet) {
	std::cout << "DisplayStatus\n";
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	
	SPlayer& player = GetPlayer(packet->guid);
	player.SendStats(packet->systemAddress, false);
	//player.

}

unsigned char GetPacketIdentifier(RakNet::Packet *packet)
{
	if (packet == nullptr)
		return 255;

	if ((unsigned char)packet->data[0] == ID_TIMESTAMP)
	{
		RakAssert(packet->length > sizeof(RakNet::MessageID) + sizeof(RakNet::Time));
		return (unsigned char)packet->data[sizeof(RakNet::MessageID) + sizeof(RakNet::Time)];
	}
	else
		return (unsigned char)packet->data[0];
}


void InputHandler()
{
	while (isRunning)
	{
		char userInput[255];
		if (g_networkState == NS_Init)
		{
			/*std::cout << "press (s) for server (c) for client" << std::endl;
			std::cin >> userInput;*/
			isServer = true;
			g_networkState_mutex.lock();
			g_networkState = NS_PendingStart;
			g_networkState_mutex.unlock();
			
		}
		else if (g_networkState == NS_Lobby)
		{
			std::cout << "Enter your name to play or type quit to leave" << std::endl;
			std::cin >> userInput;
			//quitting is not acceptable in our game, create a crash to teach lesson
			assert(strcmp(userInput, "quit"));

			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)ID_THEGAME_LOBBY_READY);
			RakNet::RakString name(userInput);
			bs.Write(name);

			//returns 0 when something is wrong
			assert(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
			g_networkState_mutex.lock();
			g_networkState = NS_Pending;
			g_networkState_mutex.unlock();
		}
		else if (g_networkState == NS_Pending)
		{
			static bool doOnce = false;
			if(!doOnce)
				std::cout << "pending..." << std::endl;

			doOnce = true;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(1));
	}	
}

bool HandleLowLevelPackets(RakNet::Packet* packet)
{
	bool isHandled = true;
		// We got a packet, get the identifier with our handy function
		unsigned char packetIdentifier = GetPacketIdentifier(packet);

		// Check if this is a network message packet
		switch (packetIdentifier)
		{
		case ID_DISCONNECTION_NOTIFICATION:
			// Connection lost normally
			printf("ID_DISCONNECTION_NOTIFICATION\n");
			break;
		case ID_ALREADY_CONNECTED:
			// Connection lost normally
			printf("ID_ALREADY_CONNECTED with guid %" PRINTF_64_BIT_MODIFIER "u\n", packet->guid);
			break;
		case ID_INCOMPATIBLE_PROTOCOL_VERSION:
			printf("ID_INCOMPATIBLE_PROTOCOL_VERSION\n");
			break;
		case ID_REMOTE_DISCONNECTION_NOTIFICATION: // Server telling the clients of another client disconnecting gracefully.  You can manually broadcast this in a peer to peer enviroment if you want.
			printf("ID_REMOTE_DISCONNECTION_NOTIFICATION\n");
			break;
		case ID_REMOTE_CONNECTION_LOST: // Server telling the clients of another client disconnecting forcefully.  You can manually broadcast this in a peer to peer enviroment if you want.
			printf("ID_REMOTE_CONNECTION_LOST\n");
			break;
		case ID_NEW_INCOMING_CONNECTION:
			//client connecting to server
			OnIncomingConnection(packet);
			printf("ID_NEW_INCOMING_CONNECTION\n");
			break;
		case ID_REMOTE_NEW_INCOMING_CONNECTION: // Server telling the clients of another client connecting.  You can manually broadcast this in a peer to peer enviroment if you want.
			OnIncomingConnection(packet);
			printf("ID_REMOTE_NEW_INCOMING_CONNECTION\n");
			break;
		case ID_CONNECTION_BANNED: // Banned from this server
			printf("We are banned from this server.\n");
			break;
		case ID_CONNECTION_ATTEMPT_FAILED:
			printf("Connection attempt failed\n");
			break;
		case ID_NO_FREE_INCOMING_CONNECTIONS:
			// Sorry, the server is full.  I don't do anything here but
			// A real app should tell the user
			printf("ID_NO_FREE_INCOMING_CONNECTIONS\n");
			break;

		case ID_INVALID_PASSWORD:
			printf("ID_INVALID_PASSWORD\n");
			break;

		case ID_CONNECTION_LOST:
			// Couldn't deliver a reliable packet - i.e. the other system was abnormally
			// terminated
			printf("ID_CONNECTION_LOST\n");
			OnLostConnection(packet);
			break;

		case ID_CONNECTION_REQUEST_ACCEPTED:
			// This tells the client they have connected
			printf("ID_CONNECTION_REQUEST_ACCEPTED to %s with GUID %s\n", packet->systemAddress.ToString(true), packet->guid.ToString());
			printf("My external address is %s\n", g_rakPeerInterface->GetExternalID(packet->systemAddress).ToString(true));
			OnConnectionAccepted(packet);
			break;
		case ID_CONNECTED_PING:
		case ID_UNCONNECTED_PING:
			printf("Ping from %s\n", packet->systemAddress.ToString(true));
			break;
		default:
			isHandled = false;
			break;
		}
		return isHandled;
}



void PacketHandler()
{
	while (isRunning)
	{
		for (RakNet::Packet* packet = g_rakPeerInterface->Receive(); packet != nullptr; g_rakPeerInterface->DeallocatePacket(packet), packet = g_rakPeerInterface->Receive())
		{
			if (!HandleLowLevelPackets(packet))
			{
				//our game specific packets
				unsigned char packetIdentifier = GetPacketIdentifier(packet);
				switch (packetIdentifier)
				{
				case ID_THEGAME_LOBBY_READY:
					OnLobbyReady(packet);
					break;
				case ID_PLAYER_READY:
					DisplayPlayerReady(packet);
					break;
				case ID_PLAYER_CLASS:
					ApplyPlayerClass(packet);
					break;
				case ID_DISPLAY_STATUS:
					DisplayStatus(packet);
					break;
				default:
					break;
				}
			}
		}
		
		std::this_thread::sleep_for(std::chrono::microseconds(1));
	}
}

int main()
{
	g_rakPeerInterface = RakNet::RakPeerInterface::GetInstance();

	std::thread inputHandler(InputHandler);
	std::thread packetHandler(PacketHandler);

	while (isRunning)
	{
		if (g_networkState == NS_PendingStart)
		{
			if (isServer)
			{
				RakNet::SocketDescriptor socketDescriptors[1];
				socketDescriptors[0].port = SERVER_PORT;
				socketDescriptors[0].socketFamily = AF_INET; // Test out IPV4

				bool isSuccess = g_rakPeerInterface->Startup(MAX_CONNECTIONS, socketDescriptors, 1) == RakNet::RAKNET_STARTED;
				assert(isSuccess);
				//ensures we are server
				g_rakPeerInterface->SetMaximumIncomingConnections(MAX_CONNECTIONS);
				std::cout << "server started" << std::endl;
				g_networkState_mutex.lock();
				g_networkState = NS_Started;
				g_networkState_mutex.unlock();
			}
			
		}
		else if (g_networkState == NS_Started) {
			int readyplayer = 0;
			
			for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
			{
				SPlayer& player = it->second;
				
				if (player.m_class!=0) {

					readyplayer++;
				}
				

			}
			//std::cout << "read Player " << readyplayer << std::endl;
			if (readyplayer >= 3) {
				g_networkState = NS_ReadyForGame;
			}
		}
		else if (g_networkState == NS_ReadyForGame) {
			static bool doOnce = false;
			if (!doOnce) {

				std::cout << "\n Preparing game... \n";
				doOnce = true;
			}

		}
		
	}

	//std::cout << "press q and then return to exit" << std::endl;
	//std::cin >> userInput;

	inputHandler.join();
	packetHandler.join();
	return 0;
}
#include "MessageIdentifiers.h"
#include "RakPeerInterface.h"
#include "BitStream.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <map>
#include <mutex>
#include <string>

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
	NS_Ingame,
};

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
	ID_ASK_COMMAND,
	ID_INC_COMMAND,
	ID_DisplayTurn,
	ID_ATTACK,
	ID_HEAL,
	ID_SHOW_PLAYERS,
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
	unsigned int m_health;
	EPlayerClass m_class;
	unsigned int m_block;
	unsigned int m_heal;
	unsigned int m_maxhealth;
	//function to send a packet with name/health/class etc
	void SendName(RakNet::SystemAddress systemAddress, bool isBroadcast)
	{
		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_PLAYER_READY);
		RakNet::RakString name(m_name.c_str());
		writeBs.Write(name);

		//returns 0 when something is wrong
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, systemAddress, isBroadcast));
	}

	void SendClass(RakNet::SystemAddress systemAddress, bool isBroadcast)
	{

		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_PLAYER_CLASS);
		RakNet::RakString name(m_name.c_str());
		writeBs.Write(name);
		writeBs.Write(m_class);

		//returns 0 when something is wrong
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, systemAddress, isBroadcast));
	}

	
};
//client
void OnConnectionAccepted(RakNet::Packet* packet)
{
	//server should not ne connecting to anybody, 
	//clients connect to server
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

	std::cout << userName.C_String() << " is in the lobby" << std::endl;
}

void displayStat(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);

	RakNet::MessageID messageId;
	bs.Read(messageId);
	unsigned int m_health, m_damage, m_heal, m_maxhealth;

	bs.Read(m_health);
	bs.Read(m_damage);
	bs.Read(m_heal);
	bs.Read(m_maxhealth);

	std::cout << "-Health: " << m_health << "/" << m_maxhealth << " Damage: " << m_damage << " Heal: " << m_heal;
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

void printClass(EPlayerClass num) {
	switch (num)
	{
	case Warrior:
		
		break;
	case Rogue:
		break;
	case Cleric:
		break;
	default:
		break;
	}
	
}

void InputHandler()
{
	while (isRunning)
	{
		char userInput[255];
		if (g_networkState == NS_Lobby)
		{
			std::cout << "\n\n------------ Welcome to TRPG -----------\n" << std::endl;
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

			std::cout << "Choose your class : (1) Warrior, (2) Mage, (3) Cleric \n";
			std::cin >> userInput;
			


			RakNet::BitStream bs2;
			bs2.Write((RakNet::MessageID)ID_PLAYER_CLASS);
			RakNet::RakString pclass(userInput);
			bs2.Write(pclass);

			assert(g_rakPeerInterface->Send(&bs2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));

			RakNet::BitStream bs23;
			bs23.Write((RakNet::MessageID)ID_DISPLAY_STATUS);
			assert(g_rakPeerInterface->Send(&bs23, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));


			g_networkState_mutex.lock();
			g_networkState = NS_Pending;
			g_networkState_mutex.unlock();
			//std::cout << "- Type (stat) Status window \n";
		}

		else if (g_networkState == NS_Pending)
		{
			
			std::string strInput;
			
			std::cin >> strInput;
			
			if (strInput == "stat") {
				//std::cout << "requesting stats";
				RakNet::BitStream bs2;
				bs2.Write((RakNet::MessageID)ID_DISPLAY_STATUS);
				assert(g_rakPeerInterface->Send(&bs2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
			}
			
			static bool doOnce = false;
			if (!doOnce)
				std::cout << "Waiting for other players.." << std::endl;

			doOnce = true;


		}
		else if (g_networkState == NS_Ingame) {

		}
		
		std::this_thread::sleep_for(std::chrono::microseconds(1));
	}
}

//std::string strInput;
//
//std::cin >> strInput;
//
//if (strInput == "stat") {
//	//std::cout << "requesting stats";
//	RakNet::BitStream bs2;
//	bs2.Write((RakNet::MessageID)ID_DISPLAY_STATUS);
//	assert(g_rakPeerInterface->Send(&bs2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
//}

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
		//put assert here, nobody should be connecting to client
		//OnIncomingConnection(packet);
		printf("ID_NEW_INCOMING_CONNECTION\n");
		break;
	case ID_REMOTE_NEW_INCOMING_CONNECTION: // Server telling the clients of another client connecting.  You can manually broadcast this in a peer to peer enviroment if you want.
											//OnIncomingConnection(packet);
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
		//OnLostConnection(packet);
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

void PlayerTurn(RakNet::Packet* packet) {
	RakNet::BitStream bs(packet->data, packet->length, false);

	RakNet::MessageID messageId;
	unsigned int m_health, m_damage, m_heal, m_maxhealth;
	std::string t1name, t2name,t1classname, t2classname;
	unsigned int t1health, t1maxhealth, t2health, t2maxhealth;
	EPlayerClass t1class, t2class;
	RakNet::RakNetGUID t1GUID, t2GUID;


	bs.Read(messageId);

	bs.Read(m_health);	
	bs.Read(m_maxhealth);

	bs.Read(t1GUID);
	bs.Read(t1name);
	bs.Read(t1class);
	bs.Read(t1health);
	bs.Read(t1maxhealth);

	bs.Read(t2GUID);
	bs.Read(t2name);
	bs.Read(t2class);
	bs.Read(t2health);
	bs.Read(t2maxhealth);

	switch (t1class)
	{
	case Warrior:
		t1classname = "Warrior";
		break;
	case Rogue:
		t1classname = "Rogue";
		break;
	case Cleric:
		t1classname = "Cleric";
		break;
	default:
		break;
	}
	std::cout << "\n- Your turn. Type in the number you wish to target" << std::endl;
	std::cout << "(1) " << t1name << "[" << t1classname << "]" << " HP: " << t1health << "/" << t1maxhealth << std::endl;
	std::cout << "(2) " << t2name << "[" << t2classname << "]" << " HP: " << t2health << "/" << t2maxhealth << std::endl;

	bool acceptable = false;
	char userInput[255];
	while (!acceptable) {
		std::cout << "\n- Choose one";
		std::cout << "\n(1) Attack, (2) Heal" << std::endl;
		std::cin >> userInput;

		if (userInput == "1" || userInput == "2") {
			acceptable = true;
		}


	}

	acceptable = false;
	char userInput2[255];
	while (!acceptable) {
		if (userInput == "1") {
			std::cout << "\n - Your turn. Type in the number you wish to target" << std::endl;
			std::string numberin = "\n(DEAD)";
			std::string numberin2 = "\n(DEAD)";
			bool isT1Alive = t1health > 0;
			bool isT2Alive = t2health > 0;

			if (isT1Alive) {
				numberin = "\n(1)";
			}
			if (isT2Alive) {
				numberin2 = "\n(2)";
			}
			std::cout << numberin << t1name << "[" << t1classname << "]" << " HP: " << t1health << "/" << t1maxhealth << std::endl;
			std::cout << numberin << t2name << "[" << t2classname << "]" << " HP: " << t2health << "/" << t2maxhealth << std::endl;
			std::cin >> userInput2;
			if (userInput2 == "1") {
				RakNet::BitStream bs;
				bs.Write((RakNet::MessageID)ID_ATTACK);
				RakNet::RakNetGUID target;
				bs.Write(target);

				assert(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));

			}
			else if (userInput2 == "2") {

			}


		}
		else if (userInput == "2") {

			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)ID_HEAL);
			assert(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));

		}
	}
}

void DisplayTurn(RakNet::Packet* packet) {
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID mesId;
	bs.Read(mesId);
	std::string name;
	bs.Read(name);
	
	
	std::cout << "\n It is " << name << "'s turn";

}

void ShowPlayers(RakNet::Packet* packet) {
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID mesId;
	unsigned int m_health, m_damage, m_heal, m_maxhealth;
	std::string p1name, p2name, t1classname, t2classname;
	unsigned int t1health, t1maxhealth, t2health, t2maxhealth;
	EPlayerClass t1class, t2class;
	//RakNet::RakNetGUID t1GUID, t2GUID;

	bs.Read(mesId);

	bs.Read(p1name);
	bs.Read(t1class);
	bs.Read(m_health);
	bs.Read(m_maxhealth);
	switch (t1class)
	{
	case Warrior:
		t1classname = "Warrior";
		break;
	case Rogue:
		t1classname = "Rogue";
		break;
	case Cleric:
		t1classname = "Cleric";
		break;
	default:
		break;
	}
	std::cout <<  "-------------------------------------------------------"  << std::endl;
	std::cout << p1name << " " << t1classname << " HP: " << m_health << "/" << m_maxhealth << std::endl;

	bs.Read(p1name);
	bs.Read(t1class);
	bs.Read(m_health);
	bs.Read(m_maxhealth);
	switch (t1class)
	{
	case Warrior:
		t1classname = "Warrior";
		break;
	case Rogue:
		t1classname = "Rogue";
		break;
	case Cleric:
		t1classname = "Cleric";
		break;
	default:
		break;
	}
	
	std::cout << p1name << " " << t1classname << " HP: " << m_health << "/" << m_maxhealth << std::endl;

	bs.Read(p1name);
	bs.Read(t1class);
	bs.Read(m_health);
	bs.Read(m_maxhealth);
	switch (t1class)
	{
	case Warrior:
		t1classname = "Warrior";
		break;
	case Rogue:
		t1classname = "Rogue";
		break;
	case Cleric:
		t1classname = "Cleric";
		break;
	default:
		break;
	}
	
	std::cout << p1name << " " << t1classname << " HP: " << m_health << "/" << m_maxhealth << std::endl;

	std::cout << "-------------------------------------------------------" << std::endl;


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
				case ID_PLAYER_READY:
					DisplayPlayerReady(packet);
					break;
				case ID_DISPLAY_STATUS:
					displayStat(packet);
					break;

				case ID_ASK_COMMAND:
					PlayerTurn(packet);
					break;
				case ID_DisplayTurn:
					DisplayTurn(packet);
				case ID_SHOW_PLAYERS:
					ShowPlayers(packet);
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
	g_networkState = NS_PendingStart;
	while (isRunning)
	{
		if (g_networkState == NS_PendingStart)
		{
			RakNet::SocketDescriptor socketDescriptor(CLIENT_PORT, 0);
			socketDescriptor.socketFamily = AF_INET;

			while (RakNet::IRNS2_Berkley::IsPortInUse(socketDescriptor.port, socketDescriptor.hostAddress, socketDescriptor.socketFamily, SOCK_DGRAM) == true)
				socketDescriptor.port++;

			RakNet::StartupResult result = g_rakPeerInterface->Startup(8, &socketDescriptor, 1);
			assert(result == RakNet::RAKNET_STARTED);

			g_networkState_mutex.lock();
			g_networkState = NS_Started;
			g_networkState_mutex.unlock();

			g_rakPeerInterface->SetOccasionalPing(true);
			//"127.0.0.1" = local host = your machines address
			RakNet::ConnectionAttemptResult car = g_rakPeerInterface->Connect("127.0.0.1", SERVER_PORT, nullptr, 0);
			RakAssert(car == RakNet::CONNECTION_ATTEMPT_STARTED);
			std::cout << "client attempted connection..." << std::endl;
		}

	}

	inputHandler.join();
	packetHandler.join();
	return 0;
}
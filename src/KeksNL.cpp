
#include <Common.h>

#include "CBitStream.h"

#include "Sockets/BerkleySocket.h"
#include "ReliabilityLayer.h"

std::string msg1 = "Keks count";
int count = 0;

class Peer;

Peer *peer1;
Peer *peer2;
Peer *peer3;

class Peer
{
private:
	keksnl::ISocket * pSocket = nullptr;

	struct System
	{
		keksnl::CReliabilityLayer reliabilityLayer;

	};

	keksnl::CReliabilityLayer reliabilityLayer;

	std::vector<System*> remoteSystems;

	std::mutex bufferMutex;
	std::queue<keksnl::InternalRecvPacket*> bufferedPacketQueue;

public:
	Peer()
	{
		// Create the local socket
		pSocket = new keksnl::CBerkleySocket();

		// Now connect our peer with the socket
		pSocket->GetEventHandler().AddEvent(keksnl::SocketEvents::RECEIVE, new keksnl::EventN<keksnl::InternalRecvPacket*>(std::bind(&Peer::OnReceive, this, std::placeholders::_1)));

		// Not we want to handle the received packets in our reliabilityLayer
		reliabilityLayer.GetEventHandler().AddEvent(keksnl::ReliabilityEvents::HANDLE_PACKET,
															new keksnl::EventN<keksnl::InternalRecvPacket*>(std::bind(&Peer::HandlePacket, this, std::placeholders::_1)));

		reliabilityLayer.GetEventHandler().AddEvent(keksnl::ReliabilityEvents::NEW_CONNECTION,
															new keksnl::EventN<keksnl::InternalRecvPacket*>(std::bind(&Peer::HandleNewConnection, this, std::placeholders::_1)));
	}


	bool OnReceive(keksnl::InternalRecvPacket* pPacket)
	{
		//std::lock_guard<std::mutex> m{bufferMutex};

		// If its a known system distribute the packet to the systems reliability layer
		for (auto &system : remoteSystems)
		{
			if (pPacket->remoteAddress == system->reliabilityLayer.GetRemoteAddress())
			{
				system->reliabilityLayer.OnReceive(pPacket);
				return true;
			}
		}

		reliabilityLayer.OnReceive(pPacket);

		return true;
	}

	keksnl::ISocket * GetSocket()
	{
		return pSocket;
	}

	void Start(unsigned short usPort)
	{
		keksnl::SocketBindArguments bi;

		bi.usPort = usPort;

		pSocket->Bind(bi);
		pSocket->StartReceiving();
	}

	bool HandlePacket(keksnl::InternalRecvPacket * packet)
	{
		keksnl::CBitStream bitStream((unsigned char*)packet->data, packet->bytesRead, true);
		keksnl::CReliabilityLayer::ReliablePacket relPacket;
		relPacket.Deserialize(bitStream);

#ifndef WIN32
		printf("Received on %p %s\n", this, relPacket.pData);
#endif
		
		char keks[100] = {0};
		sprintf_s(keks, "Keks count %d", count++);
		msg1 = keks;
		//std::chrono::milliseconds dura(2000);
		//std::this_thread::sleep_for(dura);
		packet->remoteAddress.address.addr4.sin_family = AF_INET;

		for (auto &system : remoteSystems)
		{
			if (packet->remoteAddress == system->reliabilityLayer.GetRemoteAddress())
			{
				// Send back
				Send(*system, msg1.c_str(), msg1.size() + 1);
				break;
			}
		}

		/*if (packet->pSocket)
			packet->pSocket->Send(packet->remoteAddress, msg1.c_str(), msg1.length());
		else
			printf("Socket is nullptr\n");*/
		//std::this_thread::sleep_for(std::chrono::milliseconds(10));

		return false;
	};

	bool HandleNewConnection(keksnl::InternalRecvPacket * pPacket)
	{
		System *system = new System;
		system->reliabilityLayer.SetRemoteAddress(pPacket->remoteAddress);
		system->reliabilityLayer.SetSocket(this->pSocket);

		// we want all handle events in our peer
		system->reliabilityLayer.GetEventHandler().AddEvent(keksnl::ReliabilityEvents::HANDLE_PACKET,
													new keksnl::EventN<keksnl::InternalRecvPacket*>(std::bind(&Peer::HandlePacket, this, std::placeholders::_1)));

		remoteSystems.push_back(system);

		printf("New connection at %p\n", this);
		return true;
	};

	void Send(System &peer, const char * data, size_t len)
	{
		peer.reliabilityLayer.Send((char*)data, BYTES_TO_BITS(len));

		//GetRelLayer()->GetSocket()->Send(peer.GetRelLayer()->GetSocket()->GetSocketAddress(), data, len);
	}

	void Process()
	{
		reliabilityLayer.Process();

		for (auto &peer : remoteSystems)
			peer->reliabilityLayer.Process();
	}
};

double packetsPerSec = 0;

void send1()
{
	auto sendFromPeerToPeer = [](Peer &sender, Peer &peer, const char* data, size_t numberOfBits)
	{
		keksnl::CBitStream bitStream(MAX_MTU_SIZE);
		keksnl::CReliabilityLayer::ReliablePacket packet;
		packet.mHeader.isACK = false;
		packet.mHeader.isNACK = false;
		packet.pData = (char*)data;
		packet.dataLength = BITS_TO_BYTES(numberOfBits);
		packet.Serialize(bitStream);
		//bitStream.Write(data, numberOfBitsToSend << 3);


		if (sender.GetSocket())
			sender.GetSocket()->Send(peer.GetSocket()->GetSocketAddress(), bitStream.Data(), bitStream.Size());
		else
			printf("Invalid sender at [%s:%d]\n", __FILE__, __LINE__);
	};
	while (true)
	{
		sendFromPeerToPeer(*peer1, *peer2, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));
		
		sendFromPeerToPeer(*peer1, *peer2, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));
		sendFromPeerToPeer(*peer1, *peer2, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));
		peer1->Process();
	}
}

void send2()
{
	auto sendFromPeerToPeer = [](Peer &sender, Peer &peer, const char* data, size_t numberOfBits)
	{
		keksnl::CBitStream bitStream(MAX_MTU_SIZE);
		keksnl::CReliabilityLayer::ReliablePacket packet;
		packet.mHeader.isACK = false;
		packet.mHeader.isNACK = false;
		packet.pData = (char*)data;
		packet.dataLength = BITS_TO_BYTES(numberOfBits);
		packet.Serialize(bitStream);
		//bitStream.Write(data, numberOfBitsToSend << 3);


		if (sender.GetSocket())
			sender.GetSocket()->Send(peer.GetSocket()->GetSocketAddress(), bitStream.Data(), bitStream.Size());
		else
			printf("Invalid sender at [%s:%d]\n", __FILE__, __LINE__);
	};
	while (true)
	{
		peer2->Process();
		sendFromPeerToPeer(*peer2, *peer1, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));
		sendFromPeerToPeer(*peer2, *peer1, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));
		sendFromPeerToPeer(*peer2, *peer1, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));
		sendFromPeerToPeer(*peer2, *peer1, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));

	}
}

int main()
{
	peer1 = new Peer();
	peer2 = new Peer();
	peer3 = new Peer();

	peer1->Start(9999);
	peer2->Start(10000);
	peer3->Start(10001);
	//socket2.StopReceiving();

	auto sendFromPeerToPeer = [](Peer &sender, Peer &peer, const char* data, size_t numberOfBits)
	{
		keksnl::CBitStream bitStream(MAX_MTU_SIZE);
		keksnl::CReliabilityLayer::ReliablePacket packet;
		packet.mHeader.isACK = false;
		packet.mHeader.isNACK = false;
		packet.pData = (char*)data;
		packet.dataLength = BITS_TO_BYTES(numberOfBits);
		packet.Serialize(bitStream);
		//bitStream.Write(data, numberOfBitsToSend << 3);


		if (sender.GetSocket())
			sender.GetSocket()->Send(peer.GetSocket()->GetSocketAddress(), bitStream.Data(), bitStream.Size());
		else
			printf("Invalid sender at [%s:%d]\n", __FILE__, __LINE__);
	};

	sendFromPeerToPeer(*peer1, *peer2, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));
	sendFromPeerToPeer(*peer1, *peer3, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));

	auto start = std::chrono::high_resolution_clock::now();
	
#ifdef WIN32
	auto lastTitle = GetTickCount();
#endif
	
	/*std::thread s1(send1);
	std::thread s2(send2);*/

	while (true)
	{

		if (count != 0)
			packetsPerSec = (float)count / (float)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count();

#ifdef WIN32
		if (lastTitle + 10 < GetTickCount())
		{
			char title[MAX_PATH] = {0};
			sprintf(title, "Packets per Second: %f\n", packetsPerSec);

			SetConsoleTitle(title);
			lastTitle = GetTickCount();
		}
#endif

		
		/*sendFromPeerToPeer(*peer2, *peer3, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));
		sendFromPeerToPeer(*peer1, *peer3, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));
		sendFromPeerToPeer(*peer3, *peer2, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));*/

		peer1->Process();
		peer2->Process();
		peer3->Process();
	}


	//keksnl::CBitStream bit;
	//const char  *keks = "Hallo wie gehts!";
	//bit.Write((unsigned char*)keks, strlen("Hallo wie gehts!") + 1);
	//bit.Write((unsigned char*)keks, strlen("Hallo wie gehts!") + 1);

	//int keks2 = 10000;
	//bit << keks2;

	//char * read = new char[strlen("Hallo wie gehts!") + 1];
	//bit.Read(read, strlen("Hallo wie gehts!") + 1);
	//puts(read);
	//puts("\n");

	//memset(read, 0, strlen("Hallo wie gehts!") + 1);
	//bit.Read(read, strlen("Hallo wie gehts!") + 1);
	//puts(read);
	//puts("\n");

	//int hallo = 0;
	//bit >> hallo;

	//printf("Kekse it works: %d\n", hallo);

	getchar();

	return 0;
}

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

int countPerSec = 0;

std::chrono::high_resolution_clock::time_point start;

bool ahaskdj(keksnl::InternalRecvPacket*)
{
	return true;
}


bool PublicReceiveHandler(keksnl::InternalRecvPacket* pPacket)
{

	DEBUG_LOG("Public handler");
	return true;
}

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
		pSocket->GetEventHandler().AddEvent(keksnl::SocketEvents::RECEIVE, keksnl::mkEventN(&Peer::OnReceive, this), this);

		// Not we want to handle the received packets in our reliabilityLayer
		reliabilityLayer.GetEventHandler().AddEvent(keksnl::ReliabilityEvents::HANDLE_PACKET,
													keksnl::mkEventN(&Peer::HandlePacket, this), this);

		reliabilityLayer.GetEventHandler().AddEvent(keksnl::ReliabilityEvents::NEW_CONNECTION,
															keksnl::mkEventN(&Peer::HandleNewConnection, this), this);

		DEBUG_LOG("Local ReliabilityLayer {%p}", &reliabilityLayer);
	}

	~Peer()
	{
		pSocket->GetEventHandler().RemoveEventsByOwner(this);
		reliabilityLayer.GetEventHandler().RemoveEventsByOwner(this);

		for(auto &system : remoteSystems)
		{
			system->reliabilityLayer.GetEventHandler().RemoveEventsByOwner(this);
		}

	}

	bool OnReceive(keksnl::InternalRecvPacket* pPacket)
	{
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

	bool HandleDisconnect(keksnl::SocketAddress address, keksnl::DisconnectReason reason)
	{
		for (auto &system : remoteSystems)
		{
			if (address == system->reliabilityLayer.GetRemoteAddress())
			{
				remoteSystems.erase(std::find(remoteSystems.begin(), remoteSystems.end(), system));
				remoteSystems.shrink_to_fit();

				this->reliabilityLayer.RemoveRemote(address);

				return true;
			}
		}

		return false;
	}

	keksnl::ISocket * GetSocket()
	{
		return pSocket;
	}

	void Start(const char *szAddress, unsigned short usPort)
	{
		keksnl::SocketBindArguments bi;

		bi.usPort = usPort;
		bi.szHostAddress = szAddress;

		pSocket->Bind(bi);
		pSocket->StartReceiving();
	}

	void Connect(const char * szRemoteAddress, unsigned short usPort)
	{
		keksnl::CBitStream bitStream{MAX_MTU_SIZE};

		keksnl::DatagramHeader dh;
		dh.isACK = false;
		dh.isNACK = false;
		dh.isReliable = false;
		dh.sequenceNumber = 0;

		dh.Serialize(bitStream);

		bitStream.Write(keksnl::PacketReliability::UNRELIABLE);
		bitStream.Write<unsigned short>(BITS_TO_BYTES(strlen("New connection request")));
		bitStream.Write("New connection request", BITS_TO_BYTES(strlen("New connection request")));

		keksnl::SocketAddress remoteAdd;

		memset(&remoteAdd.address.addr4, 0, sizeof(sockaddr_in));
		remoteAdd.address.addr4.sin_port = htons(usPort);

		remoteAdd.address.addr4.sin_addr.s_addr = inet_addr(szRemoteAddress);
		remoteAdd.address.addr4.sin_family = AF_INET;

		if (GetSocket())
			GetSocket()->Send(remoteAdd, bitStream.Data(), bitStream.Size());
		else
			DEBUG_LOG("Invalid sender at [%s:%d]", __FILE__, __LINE__);

		DEBUG_LOG("Send");
	}

	bool HandlePacket(keksnl::SocketAddress& remoteAddress)
	{
		//std::this_thread::sleep_for(std::chrono::milliseconds(100));

		if (count == 100)
			start = std::chrono::high_resolution_clock::now();

		countPerSec++;
		count++;
		char keks[100] = {0};
		//sprintf_s(keks, "Keks count %d", count++);
		msg1 = "Keks count %d";
		//std::chrono::milliseconds dura(2000);
		//std::this_thread::sleep_for(dura);

		for (auto system : remoteSystems)
		{
			if (remoteAddress == system->reliabilityLayer.GetRemoteAddress())
			{
				// Send back
				Send(*system, msg1.c_str(), msg1.size() + 1);
				break;
			}
		}

		/*if (packet->pSocket)
			packet->pSocket->Send(packet->remoteAddress, msg1.c_str(), msg1.length());
		else
			DEBUG_LOG("Socket is nullptr");*/
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
															keksnl::mkEventN(&Peer::HandlePacket, this), this);

		system->reliabilityLayer.GetEventHandler().AddEvent(keksnl::ReliabilityEvents::CONNECTION_LOST_TIMEOUT,
													keksnl::mkEventN(&Peer::HandleDisconnect, this), this);
		remoteSystems.push_back(system);

		DEBUG_LOG("New connection {%p} at %p", &system->reliabilityLayer, this);
		return true;
	};

	void Send(System &peer, const char * data, size_t len)
	{
		peer.reliabilityLayer.Send((char*)data, BYTES_TO_BITS(len), keksnl::PacketPriority::IMMEDIATE, keksnl::PacketReliability::RELIABLE_ORDERED);
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

#include <climits>
#include <list>

class PrintLogHander : public ILogHandler
{
	virtual void OnLog(ILogger * pLogger, LogLevel level, const char* szMsg)
	{ 
		printf("%s\n", szMsg);
	};
};

#include <chrono>

int main(int argc, char** argv)
{
	GetLogger("KeksNL")->AddHandler(new PrintLogHander);
	DEBUG_LOG("%d", sizeof(keksnl::OrderedInfo));

	if(argc > 1)
	{
		if(!strncmp(argv[1], "c", 1))
		{
			// Start the client
			const char *szAddress = argv[2];
			unsigned short usPort = atoi(argv[3]);
			Peer * pPeer = new Peer();
			pPeer->Start(0, usPort + 1);
			pPeer->Connect(szAddress, usPort);

			while(true)
			{
				pPeer->Process();
			}
		}
		else if(!strncmp(argv[1], "s", 1))
		{
			

			// Get the listening port
			unsigned short usPort = atoi(argv[2]);
			Peer * pPeer = new Peer();

			// Start the server
			pPeer->Start(0, usPort);


			while(true)
			{
				pPeer->Process();
			}
		}
		
	}


#pragma region Speed Test
#if 0
	std::random_device rd;
	std::mt19937 rnd;
	std::uniform_int_distribution<USHORT> rndShort;
	std::vector<unsigned short> vec;

	for (int i = 0; i < USHRT_MAX/10-1; ++i)
	{
		vec.push_back(rndShort(rnd));
	}

	auto st = std::chrono::high_resolution_clock::now();

	std::vector<unsigned short> sortVec;

	for (auto i : vec)
	{
		sortVec.push_back(i);
	}

	std::sort(sortVec.begin(), sortVec.end());

	DEBUG_LOG("Insert took %d ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-st).count());

	st = std::chrono::high_resolution_clock::now();
	DEBUG_LOG("Insert took %d ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - st).count());

	std::vector<unsigned short> acknowledgements{1, 2, 3, 4, 20, 21, 22, 25, 27 , 29, 30};

	// Sort the unsorted vector so we can write the ranges
	std::sort(acknowledgements.begin(), acknowledgements.end());

	keksnl::CBitStream bitStream;

	int min = -1;
	int max = 0;
	int writtenTo = 0;
	int writeCount = 0;

	// Now write the range stuff to the bitstream
	for (int i = 0; i < acknowledgements.size(); ++i)
	{
		if (acknowledgements[i] == (acknowledgements[i + 1] - 1))
		{ /* (Next-1) equals current, so its a range */

			if (min == -1)
				min = acknowledgements[i];

			max = acknowledgements[i];
		}
		else if (min == -1)
		{ /* Not a range just a single value,
		  because previous was not in row */
			min = acknowledgements[i];
			max = acknowledgements[i];
			bitStream.Write<unsigned short>(min);
			bitStream.Write<unsigned short>(max);

			DEBUG_LOG("Write -1 %d %d", acknowledgements[i], acknowledgements[i]);

			// Track the index we have written to
			writtenTo = i;
			writeCount++;

			min = -1;
		}
		else
		{
			// First diff at next so write max to current and write info to bitStream
			max = acknowledgements[i];
			bitStream.Write<unsigned short>(min);
			bitStream.Write<unsigned short>(max);

			DEBUG_LOG("Write %d %d", min, max);

			// Track the index we have written to
			writtenTo = i;
			writeCount++;

			min = -1;
		}
	}

	DEBUG_LOG("\n\n");

	acknowledgements.clear();

	keksnl::CBitStream out;
	out.Write(writeCount);
	out.Write(bitStream.Data(), bitStream.Size());


	writeCount = 0;
	out.Read(writeCount);

	for (int i = 0; i <= writeCount; ++i)
	{
		unsigned short min;
		unsigned short max;
		out.Read(min);
		out.Read(max);

		if (min < max)
		{
			for (int n = min; n <= max; ++n)
				acknowledgements.push_back(n);
		}
		else if (min == max)
			acknowledgements.push_back(min);
	}


	for (int i = 0; i < acknowledgements.size(); ++i)
	{
		if (acknowledgements[i] == (acknowledgements[i + 1] - 1))
		{ /* (Next-1) equals current, so its a range */

			if (min == -1)
				min = acknowledgements[i];

			max = acknowledgements[i];
		}
		else if (min == -1)
		{ /* Not a range just a single value,
		  because previous was not in row */
			bitStream.Write(acknowledgements[i]);
			bitStream.Write(acknowledgements[i]);

			DEBUG_LOG("Write -1 %d %d", acknowledgements[i], acknowledgements[i]);

			// Track the index we have written to
			writtenTo = i;
			writeCount++;
		}
		else
		{
			// First diff at next so write max to current and write info to bitStream
			max = acknowledgements[i];
			bitStream.Write(min);
			bitStream.Write(max);

			DEBUG_LOG("Write %d %d", min, max);

			// Track the index we have written to
			writtenTo = i;
			writeCount++;

			min = -1;
		}
	}
#endif
#pragma endregion

	DEBUG_LOG("Startup");

	peer1 = new Peer();
	peer2 = new Peer();
	peer3 = new Peer();

	peer1->Start(0, 9999);
	peer2->Start(0, 10000);
	peer3->Start(0, 10001);
	//socket2.StopReceiving();

	auto sendFromPeerToPeer = [](Peer &sender, Peer &peer, const char* data, size_t numberOfBits)
	{
		keksnl::CBitStream bitStream{MAX_MTU_SIZE};

		keksnl::DatagramHeader dh;
		dh.isACK = false;
		dh.isNACK = false;
		dh.isReliable = false;
		dh.sequenceNumber = 0;

		dh.Serialize(bitStream);

		bitStream.Write(keksnl::PacketReliability::UNRELIABLE);
		bitStream.Write<unsigned short>(BITS_TO_BYTES(numberOfBits));
		bitStream.Write(data, BITS_TO_BYTES(numberOfBits));

		if (sender.GetSocket())
			sender.GetSocket()->Send(peer.GetSocket()->GetSocketAddress(), bitStream.Data(), bitStream.Size());
		else
			DEBUG_LOG("Invalid sender at [%s:%d]", __FILE__, __LINE__);

		DEBUG_LOG("Send");
	};

	sendFromPeerToPeer(*peer1, *peer2, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));
	sendFromPeerToPeer(*peer1, *peer3, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));
	sendFromPeerToPeer(*peer3, *peer1, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));
	sendFromPeerToPeer(*peer2, *peer1, "Hallo wie gehts", BYTES_TO_BITS(sizeof("Hallo wie gehts")));

	start = std::chrono::high_resolution_clock::now().min();

#ifdef WIN32
	auto lastTitle = GetTickCount();
#endif

	/*std::thread s1(send1);
	std::thread s2(send2);*/

	while (true)
	{

		if (count != 0)
			packetsPerSec = (float)count / (float)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();

#ifdef WIN32
		//if (lastTitle + 10 < GetTickCount())
		{
			if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count() > 1000)
			{

				char title[MAX_PATH] = {0};
				sprintf(title, "Packets per Second sent by Socket: %d", countPerSec);
				countPerSec = 0;
				SetConsoleTitleA(title);
				lastTitle = GetTickCount();

				start = std::chrono::high_resolution_clock::now();
			}

		}
#endif

		//peer1->Send(*peer2, "jfdlaksj�fsdjfal�sdjfklajsdfklasdjfl", strlen("jfdlaksj�fsdjfal�sdjfklajsdfklasdjfl"));

		peer1->Process();
		peer2->Process();
		peer3->Process();
		peer1->Process();
		/*if (GetAsyncKeyState(VK_CONTROL))
		{
			_CrtDumpMemoryLeaks();
			getchar();

			__debugbreak();
		}*/
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

	//DEBUG_LOG("Kekse it works: %d", hallo);

	getchar();

	return 0;
}

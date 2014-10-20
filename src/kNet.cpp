
#include <Common.h>

#include "BitStream.h"

#include "Sockets/BerkleySocket.h"
#include "ReliabilityLayer.h"
#include "Peer.h"

#include <climits>
#include <list>
#include <chrono>

std::string msg1 = "Keks count";
int count = 0;

knet::Peer *peer1;
knet::Peer *peer2;
knet::Peer *peer3;

int countPerSec = 0;

std::chrono::high_resolution_clock::time_point start;

bool ahaskdj(knet::InternalRecvPacket*)
{
	return true;
}


bool PublicReceiveHandler(knet::InternalRecvPacket* pPacket)
{
	UNREFERENCED_PARAMETER(pPacket);
	DEBUG_LOG("Public handler");
	return true;
}

double packetsPerSec = 0;


int main(int argc, char** argv)
{
#if WIN32 && CRT_ALLOC
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

#if ENABLE_LOGGER
	GetLogger("kNet")->AddHandler([](ILogger * pLogger, LogLevel level, const char* szMsg)
	{
		printf("%s\n", szMsg);
	});
#endif

	knet::BitStream bit;

	std::string str1 = "Hallo1";

	std::string str2 = "Hallo2";

	bit.Write(str1, str2);
	bit.Write(str1);

	str1 = "";
	str2 = "";

	bit.SetReadOffset(0);

	bit.Read(str1, str2);

	DEBUG_LOG("%s %s\n", str1.c_str(), str2.c_str());

	DEBUG_LOG("%d", sizeof(knet::OrderedInfo));

	if(argc > 1)
	{
		if(!strncmp(argv[1], "c", 1))
		{
			// Start the client
			const char *szAddress = argv[2];
			unsigned short usPort = static_cast<unsigned short>(std::atoi(argv[3]));
			knet::Peer * pPeer = new knet::Peer();
			pPeer->Start(0, usPort + 1);
			pPeer->Connect(szAddress, usPort);

			for (;;)
			{
				pPeer->Process();
			}
		}
		else if(!strncmp(argv[1], "s", 1))
		{
			

			// Get the listening port
			auto usPort = static_cast<unsigned short>(std::atoi(argv[2]));
			knet::Peer * pPeer = new knet::Peer();

			// Start the server
			pPeer->Start(0, usPort);


			for (;;)
			{
				pPeer->Process();
			}
		}
		
	}

#pragma region Speed Test
#if 0
	std::random_device rd;
	std::mt19937 rnd;
	std::uniform_int_distribution<unsigned short> rndShort;
	std::vector<unsigned short> vec;

	for (int i = 0; i < 65530; ++i)
	{
		vec.push_back(i);
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

	std::vector<unsigned short> acknowledgements{sortVec};

	acknowledgements.shrink_to_fit();


	for(auto i : acknowledgements)
	{
		if(i == 0)
		{
			DEBUG_LOG("on {%p} contains 0", 0);
		}
	}

	// Sort the unsorted vector so we can write the ranges
	std::sort(acknowledgements.begin(), acknowledgements.end());

	if(acknowledgements[0] == 1)
		DEBUG_LOG("0 1 on {%p}", 0);

	knet::BitStream bitStream;

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

	knet::BitStream out;
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

	DEBUG_LOG("Startup [performance]");




	DEBUG_LOG("Size %d", sizeof(knet::ReliabilityLayer));

	peer1 = new knet::Peer();
	peer2 = new knet::Peer();
	peer3 = new knet::Peer();

	/*DEBUG_LOG("Peer1 == {%p}", peer1);
	DEBUG_LOG("Peer2 == {%p}", peer2);
	DEBUG_LOG("Peer3 == {%p}", peer3);*/

	peer1->Start("", 9999);
	peer2->Start("", 10000);
	peer3->Start("", 10001);

	peer1->Connect("127.0.0.1", 10000);
	peer1->Connect("127.0.0.1", 10001);

	//peer2->Connect("127.0.0.1", 9999);

	start = std::chrono::high_resolution_clock::now().min();

#ifdef WIN32
	auto lastTitle = GetTickCount();
#endif

	for (;;)
	{
		if (count != 0)
			packetsPerSec = (float)count / (float)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();

#ifdef WIN32
		if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count() > 1000)
		{

			char title[MAX_PATH] = {0};
			sprintf(title, "Packets per Second sent by Socket: %d", countPerSec);
			countPerSec = 0;
			SetConsoleTitleA(title);
			lastTitle = GetTickCount();

			start = std::chrono::high_resolution_clock::now();
		}
#endif

		peer1->Process();
		peer2->Process();
		peer3->Process();
	}
}

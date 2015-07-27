
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


	if(argc > 1)
	{
		if(!strncmp(argv[1], "c", 1))
		{
			// Start the client
			const char *szAddress = argv[2];
			unsigned short usPort = static_cast<unsigned short>(std::atoi(argv[3]));
			knet::Peer * pPeer = new knet::Peer();

			knet::StartupInformation startInfo;
			knet::EndPointInformation endPoint;
			endPoint.port = usPort + 1;
			startInfo.localEndPoints.push_back(endPoint);

			pPeer->Start(startInfo);
			
			
			knet::ConnectInformation connectInfo = { szAddress, usPort };
			pPeer->Connect(connectInfo);

			for (;;)
			{
				pPeer->Process();
			}
		}
		else if(!strncmp(argv[1], "s", 1))
		{ }
		
	}
}

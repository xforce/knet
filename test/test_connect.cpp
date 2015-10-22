#include <gtest/gtest.h>

#include <peer.h>

TEST(ConnectTests, BasicConnect)
{
	// Get the listening port
	auto usPort = static_cast<unsigned short>(6521);
	auto server = std::make_unique<knet::Peer>();
	auto client = std::make_unique<knet::Peer>();

	knet::StartupInformation startInfo;
	knet::EndPointInformation endPoint;
	endPoint.port = usPort;
	endPoint.host = "0.0.0.0";
	startInfo.localEndPoints.push_back(endPoint);
	startInfo.isIncoming = true;


	server->Start(startInfo);

	//
	startInfo.localEndPoints.at(0).port = usPort + 1;
	startInfo.isIncoming = false;
	//
	client->Start(startInfo);

	knet::ConnectInformation connectInfo;
	connectInfo.host = "127.0.0.1";
	connectInfo.port = usPort;

	client->Connect(connectInfo);

	bool connected = false;
	bool connectAttempTimedOut = false;

	auto start = std::chrono::system_clock::now();

	client->GetEventHandler().AddEvent(knet::PeerEvents::ConnectionAccepted, nullptr, [&]() {
		connected = true;
		return true;
	});

	while(!connected && !connectAttempTimedOut)
	{
		client->Process();
		server->Process();
		if (std::chrono::system_clock::now() > start + std::chrono::seconds(10))
			connectAttempTimedOut = true;
	}

	EXPECT_TRUE(connected);
	EXPECT_FALSE(connectAttempTimedOut);

	client->Stop();
	server->Stop();
}

TEST(ConnectTests, StayAliveConnection)
{
	// Get the listening port
	auto usPort = static_cast<unsigned short>(6521);
	auto server = std::make_unique<knet::Peer>();
	auto client = std::make_unique<knet::Peer>();

	knet::StartupInformation startInfo;
	knet::EndPointInformation endPoint;
	endPoint.port = usPort;
	endPoint.host = "0.0.0.0";
	startInfo.localEndPoints.push_back(endPoint);
	startInfo.isIncoming = true;


	server->Start(startInfo);

	//
	startInfo.localEndPoints.at(0).port = usPort + 1;
	startInfo.isIncoming = false;
	//
	client->Start(startInfo);

	knet::ConnectInformation connectInfo;
	connectInfo.host = "127.0.0.1";
	connectInfo.port = usPort;

	client->Connect(connectInfo);

	bool connectionTimedOut = false;

	auto start = std::chrono::system_clock::now();

	client->GetEventHandler().AddEvent(knet::PeerEvents::Disconnected, nullptr, [&]() {
		connectionTimedOut = true;
		return true;
	});

	while (!connectionTimedOut)
	{
		client->Process();
		server->Process();

		// Have have no way yet, to get to the timeout value
		if (std::chrono::system_clock::now() > start + std::chrono::seconds(30))
		{
			break;
		}
	}

	EXPECT_FALSE(connectionTimedOut);

	client->Stop();
	server->Stop();
}

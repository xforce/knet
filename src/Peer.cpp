/*
* Copyright (C) 2014 Crix-Dev
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Crix-Dev nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <Peer.h>

#include <CBitStream.h>

namespace keksnl
{

	enum class MessageID : char
	{ 
		CONNECTION_REQUEST,
		CONNECTION_ACCEPTED,
	};

	Peer::Peer()
	{
		// Create the local socket
		pSocket = new CBerkleySocket();

		// Now connect our peer with the socket
		pSocket->GetEventHandler().AddEvent(SocketEvents::RECEIVE, mkEventN(&Peer::OnReceive, this), this);

		// Not we want to handle the received packets in our reliabilityLayer
		reliabilityLayer.GetEventHandler().AddEvent(ReliabilityEvents::HANDLE_PACKET,
													mkEventN(&Peer::HandlePacket, this), this);

		reliabilityLayer.GetEventHandler().AddEvent(ReliabilityEvents::NEW_CONNECTION,
															mkEventN(&Peer::HandleNewConnection, this), this);

		DEBUG_LOG("Local ReliabilityLayer {%p}", &reliabilityLayer);
	}

	Peer::~Peer()
	{
		pSocket->GetEventHandler().RemoveEventsByOwner(this);
		reliabilityLayer.GetEventHandler().RemoveEventsByOwner(this);

		for(auto &system : remoteSystems)
		{
			system->reliabilityLayer.GetEventHandler().RemoveEventsByOwner(this);
		}
	}


	void Peer::Connect(const char * szRemoteAddress, unsigned short usPort)
	{
		CBitStream bitStream{MAX_MTU_SIZE};

		DatagramHeader dh;
		dh.isACK = false;
		dh.isNACK = false;
		dh.isReliable = false;
		dh.sequenceNumber = 0;

		dh.Serialize(bitStream);

		bitStream.Write(PacketReliability::UNRELIABLE);
		bitStream.Write<unsigned short>(sizeof(MessageID::CONNECTION_REQUEST));
		bitStream.Write(MessageID::CONNECTION_REQUEST);

		SocketAddress remoteAdd;

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

	void Peer::Send(System &peer, const char * data, size_t len)
	{
		peer.reliabilityLayer.Send((char*)data, BYTES_TO_BITS(len), PacketPriority::IMMEDIATE, PacketReliability::RELIABLE_ORDERED);
	}



#pragma region EventHandler stuff

	bool Peer::OnReceive(InternalRecvPacket* pPacket)
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

	bool Peer::HandleDisconnect(SocketAddress address, DisconnectReason reason)
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

	bool Peer::HandlePacket(ReliablePacket &packet, SocketAddress& remoteAddress)
	{
		//std::this_thread::sleep_for(std::chrono::milliseconds(100));

		static int count = 0;
		static std::string msg1 = "";

		char * pData = packet.Data();

		if ((MessageID)pData[0] == MessageID::CONNECTION_REQUEST)
		{
			DEBUG_LOG("Connection request");

			CBitStream bitStream{MAX_MTU_SIZE};

			DatagramHeader dh;
			dh.isACK = false;
			dh.isNACK = false;
			dh.isReliable = false;
			dh.sequenceNumber = 0;

			dh.Serialize(bitStream);

			bitStream.Write(PacketReliability::UNRELIABLE);
			bitStream.Write<unsigned short>(sizeof(MessageID::CONNECTION_ACCEPTED));
			bitStream.Write(MessageID::CONNECTION_ACCEPTED);

			if (GetSocket())
				GetSocket()->Send(remoteAddress, bitStream.Data(), bitStream.Size());
			else
				DEBUG_LOG("Invalid sender at [%s:%d]", __FILE__, __LINE__);

			DEBUG_LOG("Send");
		}
		else if ((MessageID)pData[0] == MessageID::CONNECTION_ACCEPTED)
		{
			DEBUG_LOG("Remote accepted our connection");

			//countPerSec++;
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
DEBUG_LOG("Send back");

					// Send back
					Send(*system, msg1.c_str(), msg1.size() + 1);
					break;
				}
			}
		}
		else
		{
			//countPerSec++;
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
		}

		return false;
	};

	bool Peer::HandleNewConnection(InternalRecvPacket * pPacket)
	{
		System *system = new System;
		system->reliabilityLayer.SetRemoteAddress(pPacket->remoteAddress);
		system->reliabilityLayer.SetSocket(this->pSocket);

		// we want all handle events in our peer
		system->reliabilityLayer.GetEventHandler().AddEvent(ReliabilityEvents::HANDLE_PACKET,
															mkEventN(&Peer::HandlePacket, this), this);

		system->reliabilityLayer.GetEventHandler().AddEvent(ReliabilityEvents::CONNECTION_LOST_TIMEOUT,
													mkEventN(&Peer::HandleDisconnect, this), this);
		remoteSystems.push_back(system);

		DEBUG_LOG("New connection {%p} at %p", &system->reliabilityLayer, this);
		return true;
	};
#pragma endregion
};
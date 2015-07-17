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

#include <BitStream.h>

namespace knet
{
	Peer::Peer() noexcept
	{
		// Create the local socket
		_socket = std::make_shared<BerkleySocket>();

		// Now connect our peer with the socket
		_socket->GetEventHandler().AddEvent(SocketEvents::RECEIVE, this, &Peer::OnReceive, this);

		// Now we want to handle the received packets in our reliabilityLayer
		reliabilityLayer.GetEventHandler().AddEvent(ReliabilityEvents::HANDLE_PACKET, this, 
													&Peer::HandlePacket, this);

		reliabilityLayer.GetEventHandler().AddEvent(ReliabilityEvents::NEW_CONNECTION, this, 
															&Peer::HandleNewConnection, this);

		DEBUG_LOG("Local ReliabilityLayer {%p}", &reliabilityLayer);
	}

	Peer::~Peer() noexcept
	{
		_socket->GetEventHandler().RemoveEventsByOwner(this);
		reliabilityLayer.GetEventHandler().RemoveEventsByOwner(this);

		for(auto &system : remoteSystems)
		{
			system->reliabilityLayer.GetEventHandler().RemoveEventsByOwner(this);
		}
	}

	std::shared_ptr<knet::ISocket> Peer::GetSocket() noexcept
	{
		return _socket;
	}

	void Peer::Start(const std::string & strAddress, uint16_t usPort) noexcept
	{
		knet::SocketBindArguments bi;

		bi.usPort = usPort;
		bi.szHostAddress = strAddress;

		_socket->Bind(bi);
		_socket->StartReceiving();
	}


	void Peer::Connect(const std::string &strRemoteAddress, uint16_t usPort) noexcept
	{
		BitStream bitStream{MAX_MTU_SIZE};

		DatagramHeader dh;
		dh.isACK = false;
		dh.isNACK = false;
		dh.isReliable = false;
		dh.sequenceNumber = 0;

		dh.Serialize(bitStream);

		bitStream.Write(PacketReliability::UNRELIABLE);
		bitStream.Write<uint16_t>(sizeof(MessageID::CONNECTION_REQUEST));
		bitStream.Write(MessageID::CONNECTION_REQUEST);

		SocketAddress remoteAdd = { 0 };

		memset(&remoteAdd.address.addr4, 0, sizeof(sockaddr_in));
		remoteAdd.address.addr4.sin_port = htons(usPort);

		remoteAdd.address.addr4.sin_addr.s_addr = inet_addr(strRemoteAddress.c_str());
		remoteAdd.address.addr4.sin_family = AF_INET;

		// Send the connection request to the remote
		if (_socket)
			_socket->Send(remoteAdd, bitStream.Data(), bitStream.Size());
		else
			DEBUG_LOG("Invalid sender at [%s:%d]", __FILE__, __LINE__);
	}

	void Peer::Process() noexcept
	{
		reliabilityLayer.Process();

		if (reorderRemoteSystems)
		{
			std::sort(std::begin(remoteSystems), std::end(remoteSystems), [](const std::shared_ptr<System> &systemLeft, const std::shared_ptr<System> &systemRight) {
				UNREFERENCED_PARAMETER(systemRight);
				return systemLeft->isActive;
			});

			reorderRemoteSystems = false;
		}


		if (activeSystems > 0)
		{
			for (auto &peer : remoteSystems)
			{
				// Break, the list is sorted
				if (!peer->isActive)
					break;

				peer->reliabilityLayer.Process();
			}
		}
		else
		{
			// We have not active connections, so sleep a short amount of time
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

	}

	void Peer::Send(System &peer, const char * data, size_t len, bool im) noexcept
	{
		if (isConnected)
		{
			peer.reliabilityLayer.Send(data, BytesToBits(len), (im ? PacketPriority::MEDIUM : PacketPriority::HIGH), PacketReliability::RELIABLE);
		}
		else
			DEBUG_LOG("Not connected");
	}



#pragma region EventHandler stuff

	bool Peer::OnReceive(InternalRecvPacket* pPacket) noexcept
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

	bool Peer::HandleDisconnect(SocketAddress address, DisconnectReason reason) noexcept
	{
		UNREFERENCED_PARAMETER(reason);

		for (auto &system : remoteSystems)
		{
			if (address == system->reliabilityLayer.GetRemoteAddress())
			{
				system->isConnected = false;
				system->isActive = false;

				this->reorderRemoteSystems = true;

				this->reliabilityLayer.RemoveRemote(address);

				--activeSystems;

				return true;
			}
		}
		return false;
	}

	bool Peer::HandlePacket(ReliablePacket &packet, SocketAddress& remoteAddress) noexcept
	{
		auto pData = packet.Data();

		if ((MessageID)pData[0] == MessageID::CONNECTION_REQUEST)
		{
			DEBUG_LOG("Connection request");

			BitStream bitStream{MAX_MTU_SIZE};

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

			this->isConnected = true;

			// Makr the remote as connected
			for (auto &system : remoteSystems)
			{
				if (remoteAddress == system->reliabilityLayer.GetRemoteAddress())
				{
					system->isConnected = true;
					break;
				}
			}
		}
		else if ((MessageID)pData[0] == MessageID::CONNECTION_REFUSED)
		{
			this->isConnected = false;
			DEBUG_LOG("The remote refused our connection request");
		}
		else
		{
			
		}

		return false;
	};

	bool Peer::HandleNewConnection(InternalRecvPacket * pPacket) noexcept
	{
		if (remoteSystems.size() >= maxConnections)
		{
			/* We dont want to create a new system, so we have to build the packet manually :D */
			BitStream bitStream{MAX_MTU_SIZE};

			DatagramHeader dh;
			dh.isACK = false;
			dh.isNACK = false;
			dh.isReliable = false;
			dh.sequenceNumber = 0;

			dh.Serialize(bitStream);

			bitStream.Write(PacketReliability::UNRELIABLE);
			bitStream.Write<unsigned short>(sizeof(MessageID::CONNECTION_REFUSED));
			bitStream.Write(MessageID::CONNECTION_REFUSED);

			this->_socket->Send(pPacket->remoteAddress, bitStream.Data(), bitStream.Size());
			return false;
		}

		/* Clean up before a new connection is added */
		remoteSystems.erase(std::remove_if(std::begin(remoteSystems), std::end(remoteSystems), [](const std::shared_ptr<System> &system) {
			if (system->isActive)
				return false;
			else
			{
				return true;
			}
		}), std::end(remoteSystems));


		auto system = std::make_shared<System>();
		system->reliabilityLayer.SetRemoteAddress(pPacket->remoteAddress);
		system->reliabilityLayer.SetSocket(this->_socket);

		// we want all handle events in our peer
		system->reliabilityLayer.GetEventHandler().AddEvent(ReliabilityEvents::HANDLE_PACKET, this,
															&Peer::HandlePacket, this);

		system->reliabilityLayer.GetEventHandler().AddEvent(ReliabilityEvents::CONNECTION_LOST_TIMEOUT, this,
													&Peer::HandleDisconnect, this);
		remoteSystems.push_back(system);

		++activeSystems;

		DEBUG_LOG("New connection {%p} at %p", &system->reliabilityLayer, this);
		return true;
	};
#pragma endregion
};

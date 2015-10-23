// Copyright 2015 the kNet authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "internal/event_handler.h"

#include "sockets/berkley_socket.h"
#include "reliability_layer.h"

namespace knet
{
	// TODO: clean up
	struct SystemAddress
	{
		std::string address;

		bool IsUnassigned()
		{
			return address == "unassigned";
		}

		bool operator ==(const SystemAddress &other) const
		{
			return other.address == address;
		}
	};

	struct EndPointInformation
	{
		std::string host;
		uint16_t port;
	};

	struct StartupInformation
	{
		int maxConnections = 1;
		std::string password;
		std::vector<EndPointInformation> localEndPoints;
		bool isIncoming = false; // Allowed to accept incoming connections;
	};

	struct ConnectInformation
	{
		std::string host;
		uint16_t port;
		std::string password;
	};

	enum class PeerEvents : uint8_t
	{
		ConnectionAccepted,
		Disconnected
	};

	class Peer
	{
	private:
		std::shared_ptr<knet::ISocket> _socket = nullptr;

		struct System
		{
			knet::ReliabilityLayer reliabilityLayer;
			bool isConnected = false;
			bool isActive = true;
			std::chrono::steady_clock::time_point lastPing = std::chrono::steady_clock::now();
		};

		knet::ReliabilityLayer reliabilityLayer;

		std::vector<std::shared_ptr<System>> remoteSystems;

		std::mutex bufferMutex;
		std::queue<knet::InternalRecvPacket*> bufferedPacketQueue;

		uint32_t maxConnections = 5;

		bool isConnected = false;
		bool reorderRemoteSystems = true;
		uint32_t activeSystems = 0;


		knet::internal::EventHandler<PeerEvents> _eventHandler;

		System& GetSystemByAddress(SystemAddress address)
		{}
	public:
		Peer() noexcept;
		virtual ~Peer() noexcept;

		std::weak_ptr<knet::ISocket> GetSocket() noexcept;

		decltype(_eventHandler)& GetEventHandler() noexcept
		{
			return _eventHandler;
		}

		void Start(const StartupInformation&) noexcept;
		void Connect(const ConnectInformation&) noexcept;
		void Stop();

		void Process() noexcept;
	private:
		/* Event handlers */
		bool OnReceive(knet::InternalRecvPacket* pPacket) noexcept;
		bool HandleDisconnect(knet::SocketAddress address, knet::DisconnectReason reason) noexcept;
		bool HandleNewConnection(knet::InternalRecvPacket * pPacket) noexcept;
		bool HandlePacket(knet::ReliablePacket &packet, knet::SocketAddress& remoteAddress) noexcept;


		void SendInternalPing(const SocketAddress&);
	};

};

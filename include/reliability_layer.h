// Copyright 2015 the kNet authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "sockets/berkley_socket.h"

#include "internal/event_handler.h"
#include "internal/datagram_header.h"
#include "internal/reliable_packet.h"
#include "internal/datagram_packet.h"

// STL/CRT includes
#include <mutex>
#include <queue>
#include <bitset>
#include <unordered_map>
#include <array>

namespace knet
{
	enum class ReliabilityEvents : uint8_t
	{
		RECEIVE = 0,
		DISCONNECTED,
		HANDLE_PACKET,
		NEW_CONNECTION,
		MAX_EVENTS,
	};

	enum class DisconnectReason : uint8_t
	{
		TIMEOUT = 0,
	};

	enum class MessageID : uint8_t
	{
		CONNECTION_REQUEST,
		CONNECTION_ACCEPTED,
		CONNECTION_REFUSED,
		INTERNAL_PING,
		INTERNAL_PING_RESPONSE
	};



	class FlowControlHelper
	{
	private:
		int32_t sequenceNumber = 0;
		uint16_t splitNumber = 0;

	public:

		decltype(sequenceNumber) GetSequenceNumber()
		{
			if (sequenceNumber >= std::numeric_limits<decltype(sequenceNumber)>::max())
				sequenceNumber = 0;

			return sequenceNumber++;
		}

		decltype(sequenceNumber) GetCurrentSequenceNumber()
		{
			return sequenceNumber;
		}

		decltype(splitNumber) GetSplitPacketIndex()
		{
			if (splitNumber >= std::numeric_limits<decltype(splitNumber)>::max())
				splitNumber = 0;

			return splitNumber++;
		}
	};

	//! Reliability layer.
	/*!
	  Internal class
	*/
	class ReliabilityLayer
	{
	public:

		/* Types/structs used internally in Reliability Layer */
		struct RemoteSystem
		{
			std::weak_ptr<ISocket> _socket;
			SocketAddress address;

			bool operator==(const RemoteSystem& system) const
			{
				return(address == system.address);
			}
		};

	private:
		ReliabilityLayer& operator=(const ReliabilityLayer&) = delete;
		ReliabilityLayer(const ReliabilityLayer&) = delete;

		std::weak_ptr<ISocket> m_pSocket;
		SocketAddress m_RemoteSocketAddress;

		OrderedChannelType orderingChannel = 0;

		FlowControlHelper flowControlHelper;

		internal::EventHandler<ReliabilityEvents> eventHandler;

		using milliSecondsPoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;

		std::chrono::milliseconds _timeout = std::chrono::milliseconds(10000);
		milliSecondsPoint firstUnsentAck;
		milliSecondsPoint lastReceiveFromRemote;

		std::mutex bufferMutex;

		std::queue<InternalRecvPacket*> bufferedPacketQueue;

		std::vector<RemoteSystem> remoteList;
		std::vector<SequenceNumberType> acknowledgements;
		std::vector<std::pair<milliSecondsPoint, std::unique_ptr<DatagramPacket>>> resendBuffer;

		std::array<OrderedIndexType, 255> orderingIndex;
		std::array<SequenceIndexType, 255> sequencingIndex;
		std::array<std::vector<ReliablePacket>, static_cast<std::size_t>(PacketPriority::IMMEDIATE)> sendBuffer;
		std::array<std::vector<ReliablePacket>, 255> orderedPacketBuffer;

		std::array<OrderedIndexType, 255> lastOrderedIndex;
		std::array<SequenceIndexType, 255> highestSequencedReadIndex;


		std::unordered_map<uint16_t, std::vector<ReliablePacket>> splitPacketBuffer;
	private:
		/* Methods */
		void SendACKs();


		bool ProcessPacket(InternalRecvPacket *pPacket, std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> &curTime);

		void ProcessResend(milliSecondsPoint &curTime);
		void ProcessOrderedPackets(milliSecondsPoint &curTime);
		void ProcessSend(milliSecondsPoint &curTime);

		bool SplitPacket(ReliablePacket & packet, DatagramPacket ** pDatagramPacket);

	public:
		ReliabilityLayer();
		ReliabilityLayer(std::weak_ptr<ISocket>);
		virtual ~ReliabilityLayer();

		void Send(const char *, size_t, PacketPriority = PacketPriority::MEDIUM, PacketReliability = PacketReliability::RELIABLE);

		void Process();

		bool OnReceive(InternalRecvPacket *packet);

		InternalRecvPacket* PopBufferedPacket();

		inline decltype(eventHandler) &GetEventHandler()
		{
			return eventHandler;
		}


		void RemoveRemote(const SocketAddress &remoteAddress);

		/* Getters/Setters */


		//! Get the channel used to order sent packets
		/*!
		\return The current channel on which the remote reliability layer will order incoming ordered packets
		Packets sent before may used a different channel
		*/
		OrderedChannelType GetOrderingChannel() const;

		//! Sets the channel used to order sent packets
		/*!
		\param[in] ucChannel The channel on which the next ordered packets will be ordered on the remote reliability layer.
		*/
		void SetOrderingChannel(OrderedChannelType ucChannel);


		//! Gets the socket used to send packets
		/*!
		\return Socket used to send packets
		*/
		std::weak_ptr<ISocket> GetSocket() const;

		//! Sets the socket used to send packets
		/*!
		\param[in] _socket Socket used to send packets
		*/
		void SetSocket(std::weak_ptr<ISocket> _socket);


		//! Gets the remote Socket address
		/*!
		\return The remote address is the connection endpoint to which the packets will be send
		*/
		const SocketAddress & GetRemoteAddress() const;

		//! Sets the remote Socket address which is the connection endpoint
		/*!
		\param[in] socketAddress
		*/
		void SetRemoteAddress(SocketAddress &socketAddress);


		//! Sets the timeout for the connections
		/*!
		\param[in] time The time in ms after a connection will be closed if no data was received
		*/
		void SetTimeout(const std::chrono::milliseconds &time);

		//! Gets the timeout for the connections
		/*!
		\return The time after a connection will be closed if no data was received
		*/
		const std::chrono::milliseconds& GetTimeout() const;

	};

}

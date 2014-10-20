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

#pragma once

#include "Common.h"

#include "Sockets/BerkleySocket.h"
#include "EventHandler.h"

#include "DatagramHeader.h"
#include "ReliablePacket.h"
#include "DatagramPacket.h"

#include <mutex>
#include <queue>
#include <bitset>
#include <unordered_map>

namespace knet
{
	enum class ReliabilityEvents : uint8
	{
		RECEIVE = 0,
		CONNECTION_LOST_TIMEOUT,
		HANDLE_PACKET,
		NEW_CONNECTION,
		MAX_EVENTS,
	};

	enum class DisconnectReason : uint8
	{
		TIMEOUT = 0,
	};

	enum class MessageID : uint8
	{
		CONNECTION_REQUEST,
		CONNECTION_ACCEPTED,
		CONNECTION_REFUSED,
		INTERNAL_PING,
	};



	class FlowControlHelper
	{
	private:
		int32 sequenceNumber = 0;
		uint16 splitNumber = 0;

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
			std::shared_ptr<ISocket> _socket = nullptr;
			SocketAddress address;

			bool operator==(const RemoteSystem& system) const
			{
				return(_socket == system._socket && address == system.address);
			}
		};

	private:
		ReliabilityLayer& operator=(const ReliabilityLayer&) = delete;
		ReliabilityLayer(const ReliabilityLayer&) = delete;

		std::shared_ptr<ISocket> m_pSocket = nullptr;
		SocketAddress m_RemoteSocketAddress;

		uint8 orderingChannel = 0;

		FlowControlHelper flowControlHelper;

		EventHandler<ReliabilityEvents> eventHandler;

		using milliSecondsPoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;

		std::chrono::milliseconds m_msTimeout = std::chrono::milliseconds(10000);
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
		
		std::array<uint16, 255> lastOrderedIndex;
		std::array<uint16, 255> highestSequencedReadIndex;


		std::unordered_map<uint16, std::vector<ReliablePacket>> splitPacketBuffer;
	private:
		/* Methods */
		void SendACKs();


		bool ProcessPacket(InternalRecvPacket *pPacket, std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> &curTime);

		void ProcessResend(milliSecondsPoint &curTime);
		void ProcessOrderedPackets(milliSecondsPoint &curTime);
		void ProcessSend(milliSecondsPoint &curTime);

		bool SplitPacket(ReliablePacket & packet, DatagramPacket ** pDatagramPacket);

	public:
		ReliabilityLayer(ISocket * _socket = nullptr);
		virtual ~ReliabilityLayer();

		void Send(const char *data, size_t numberOfBitsToSend, PacketPriority priority = PacketPriority::MEDIUM, PacketReliability reliability = PacketReliability::RELIABLE);

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
		uint8 GetOrderingChannel() const;

		//! Sets the channel used to order sent packets
		/*!
		\param[in] ucChannel The channel on which the next ordered packets will be ordered on the remote reliability layer.
		*/
		void SetOrderingChannel(uint8 ucChannel);


		//! Gets the socket used to send packets
		/*!
		\return Socket used to send packets
		*/
		std::shared_ptr<ISocket> GetSocket() const;

		//! Sets the socket used to send packets
		/*!
		\param[in] _socket Socket used to send packets
		*/
		void SetSocket(std::shared_ptr<ISocket> _socket);

		
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

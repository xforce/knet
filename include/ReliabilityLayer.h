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

#include <mutex>
#include <queue>
#include <bitset>

namespace keksnl
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

	enum class PacketPriority : uint8
	{
		/* Will skip send buffer, so you have the control when this packet is sent*/
		IMMEDIATE = 0,
		HIGH,
		MEDIUM,
		LOW
	};

	enum class PacketReliability : uint8
	{
		UNRELIABLE = 0,

		RELIABLE,
		RELIABLE_ORDERED,

		MAX
	};

	class CFlowControlHelper
	{
	private:
		int32 sequenceNumber = 0;

	public:

		int GetSequenceNumber()
		{
			if (sequenceNumber >= std::numeric_limits<decltype(sequenceNumber)>::max())
				sequenceNumber = 0;

			return sequenceNumber++;
		}
	};


	typedef int SequenceNumberType;

	class DatagramHeader
	{
	public:
		bool isACK;
		bool isNACK;
		bool isReliable = true;
		SequenceNumberType sequenceNumber = 0;


		virtual void Serialize(CBitStream & bitStream)
		{
			bitStream.Write(isACK);
			bitStream.Write(isNACK);
			bitStream.Write(isReliable);

			// Now fill to 1 byte to improve performance
			// This can later be used for more information in the header
			bitStream.Write0();
			bitStream.Write0();
			bitStream.Write0();
			bitStream.Write0();
			bitStream.Write0();

			/* To save bandwith for ack/nack */
			if (!isACK && !isNACK && isReliable)
				bitStream.Write(sequenceNumber);
		}

		virtual void Deserialize(CBitStream & bitStream)
		{
			bitStream.Read(isACK);
			bitStream.Read(isNACK);
			bitStream.Read(isReliable);
			bitStream.SetReadOffset(bitStream.ReadOffset() + 5);

			if (!isACK && !isNACK && isReliable)
				bitStream.Read(sequenceNumber);
		}

		size_t GetSizeToSend()
		{
			return 1 + ((!isACK && !isNACK && isReliable) ? sizeof(sequenceNumber) : 0);
		}
	};

	struct OrderedInfo
	{
		uint16 index;
		uint8 channel;
	};

	struct ReliablePacket
	{
	private:
		bool selfAllocated = false;
		char * pData = nullptr;
		uint16 dataLength = 0;

	public:
		PacketReliability reliability = PacketReliability::UNRELIABLE;
		PacketPriority priority;
		OrderedInfo orderedInfo;

		ReliablePacket()
		{

		}

		char * Data()
		{
			return pData;
		}

		decltype(dataLength) Size()
		{
			return dataLength;
		}

		ReliablePacket(char * data, size_t length)
		{
			pData = (char*)(malloc(length));
			memcpy(pData, data, length);
			selfAllocated = true;
			dataLength = length;
		}

		ReliablePacket(ReliablePacket &&other)
		{
			this->pData = other.pData;
			this->selfAllocated = other.selfAllocated;
			this->priority = other.priority;
			this->reliability = other.reliability;
			this->dataLength = other.dataLength;
			this->orderedInfo = other.orderedInfo;

			other.pData = nullptr;
		}

		~ReliablePacket()
		{
			if (pData && selfAllocated)
				free(pData);
		}

		void Serialize(CBitStream &bitStream)
		{
			bitStream.Write(reliability);

			if(reliability == PacketReliability::RELIABLE_ORDERED)
			{
				bitStream.Write(orderedInfo);
			}

			bitStream.Write(dataLength);
			bitStream.Write(pData, dataLength);
		}

		void Deserialize(CBitStream &bitStream)
		{
			bitStream.Read(reliability);

			if(reliability == PacketReliability::RELIABLE_ORDERED)
			{
				bitStream.Read(orderedInfo);
			}

			bitStream.Read(dataLength);

			if (pData)
				free(pData);

			pData = (char*)(malloc(dataLength));

			selfAllocated = true;


#if WIN32
			if (dataLength > bitStream.Size())
				__debugbreak();
#endif

			bitStream.Read(pData, dataLength);
		}

		size_t GetSizeToSend()
		{
			return sizeof(dataLength)+dataLength;
		}

		ReliablePacket & operator=(const ReliablePacket &packet)
		{
			this->dataLength = packet.dataLength;
			this->pData = packet.pData;
			this->priority = packet.priority;
			this->selfAllocated = false;
			this->reliability = packet.reliability;
			this->dataLength = packet.dataLength;
			this->orderedInfo = packet.orderedInfo;
			

			return *this;
		}
	};

	struct DatagramPacket
	{
		DatagramHeader header;

		std::vector<ReliablePacket> packets;

		DatagramPacket()
		{
			packets.reserve(3);
		}

		void Serialize(CBitStream & bitStream)
		{
			header.Serialize(bitStream);

			if (!header.isACK && !header.isNACK)
				for (auto &packet : packets)
					packet.Serialize(bitStream);

		}

		void Deserialze(CBitStream & bitStream)
		{
			header.Deserialize(bitStream);

			if (!header.isACK && !header.isNACK)
			{
				while (bitStream.ReadOffset() < BYTES_TO_BITS(bitStream.Size()))
				{
					ReliablePacket packet;
					packet.Deserialize(bitStream);
					packets.push_back(std::move(packet));
				}
			}
		}

		size_t GetSizeToSend()
		{
			size_t size = 0;
			for (auto &packet : packets)
			{
				size += packet.GetSizeToSend();
			}

			return size+header.GetSizeToSend();
		}
	};


	class CReliabilityLayer
	{
	public:

		/* Types/structs used internally in Reliability Layer */
		struct RemoteSystem
		{
			ISocket * pSocket = nullptr;
			SocketAddress address;

			bool operator==(const RemoteSystem& system) const
			{
				return(pSocket == system.pSocket && address == system.address);
			}
		};

	private:
		ISocket * m_pSocket = nullptr;
		SocketAddress m_RemoteSocketAddress;

		uint8 orderingChannel = 0;

		CFlowControlHelper flowControlHelper;

		EventHandler<ReliabilityEvents> eventHandler;

		std::chrono::milliseconds m_msTimeout = std::chrono::milliseconds(10000);
		std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> firstUnsentAck;
		std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> lastReceiveFromRemote;

		std::mutex bufferMutex;

#if WIN32
		std::array<uint32, 255> orderingIndex;
#else
		std::array<uint32, std::numeric_limits<decltype(orderingChannel)>::max()> orderingIndex;
#endif

		std::queue<InternalRecvPacket*> bufferedPacketQueue;


		std::vector<RemoteSystem> remoteList;

		std::vector<SequenceNumberType> acknowledgements;


		std::vector<ReliablePacket> sendBuffer;

		std::vector<std::pair<std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>, DatagramPacket*>> resendBuffer;

#if WIN32
		std::array<std::vector<ReliablePacket>, 255> orderedPacketBuffer;
#else
		std::array<std::vector<ReliablePacket>, std::numeric_limits<decltype(orderingChannel)>::max()> orderedPacketBuffer;
#endif


#if WIN32
		std::array<uint16, 255> lastOrderedIndex;
#else
		std::array<uint16, std::numeric_limits<decltype(orderingChannel)>::max()> lastOrderedIndex;
#endif

	private:
		/* Methods */
		void SendACKs();


		bool ProcessPacket(InternalRecvPacket *pPacket);

	public:
		CReliabilityLayer(ISocket * pSocket = nullptr);
		virtual ~CReliabilityLayer();

		void Send(char *data, size_t numberOfBitsToSend, PacketPriority priority = PacketPriority::MEDIUM, PacketReliability reliability = PacketReliability::RELIABLE);

		void Process();

		bool OnReceive(InternalRecvPacket *packet);

		InternalRecvPacket* PopBufferedPacket();

		decltype(eventHandler) &GetEventHandler()
		{
			return eventHandler;
		}


		void RemoveRemote(const SocketAddress &remoteAddress);

		/* Getters/Setters */


		uint8 GetOrderingChannel();
		void SetOrderingChannel(uint8 ucChannel);

		ISocket * GetSocket();
		void SetSocket(ISocket * pSocket);

		const SocketAddress & GetRemoteAddress();

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
		const std::chrono::milliseconds& GetTimeout();

	};

}

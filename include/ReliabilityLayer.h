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
	};

	enum PacketPriority : uint8
	{
		/* Will skip send buffer, so you have the control when this packet is sent*/
		LOW,
		MEDIUM,
		HIGH,
		IMMEDIATE,
		MAX,
	};

	enum class PacketReliability : uint8
	{
		UNRELIABLE = 0,

		RELIABLE,
		RELIABLE_ORDERED,

		MAX,
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


	typedef int SequenceNumberType;

	class DatagramHeader
	{
	public:
		bool isACK;
		bool isNACK;
		bool isReliable = true;
		bool isSplit = false;
		SequenceNumberType sequenceNumber = 0;


		virtual void Serialize(BitStream & bitStream)
		{
			bitStream.Write(isACK);
			bitStream.Write(isNACK);
			bitStream.Write(isReliable);
			bitStream.Write(isSplit);

			// Now fill to 1 byte to improve performance
			// This can later be used for more information in the header
			bitStream.AddWriteOffset(4);

			/* To save bandwith for ack/nack */
			if (!isACK && !isNACK && isReliable)
				bitStream.Write(sequenceNumber);
		}

		virtual void Deserialize(BitStream & bitStream)
		{
			bitStream.Read(isACK);
			bitStream.Read(isNACK);
			bitStream.Read(isReliable);
			bitStream.Read(isSplit);

			bitStream.SetReadOffset(bitStream.ReadOffset() + 4);

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

	struct SplitInfo
	{
		uint16 index;
		uint16 packetIndex;
		uint8 isEnd = false;
	};

	struct ReliablePacket
	{
	private:
		std::unique_ptr<char> pData = nullptr;
		uint16 dataLength = 0;

	public:
		PacketReliability reliability = PacketReliability::UNRELIABLE;
		PacketPriority priority;
		
		OrderedInfo orderedInfo;

		SplitInfo splitInfo;
	
		SequenceNumberType sequenceNumber = 0;
		SocketAddress socketAddress;

		bool isSplit = false;

		ReliablePacket()
		{

		}

		ReliablePacket(const ReliablePacket &other) = delete;

		char * Data()
		{
			return pData.get();
		}

		decltype(dataLength) Size()
		{
			return dataLength;
		}

		ReliablePacket(char * data, ::size_t length)
		{
			
			pData = std::unique_ptr<char>{new char[length]};
			memcpy(pData.get(), data, length);
			dataLength = length;
		}

		ReliablePacket(ReliablePacket &&other)
		{
#if WIN32 /* Visual Studio stable_sort calls self move assignment operator */
			if (this == &other)
				return;
#endif

			this->pData = std::move(other.pData);
			this->priority = other.priority;
			this->reliability = other.reliability;
			this->dataLength = other.dataLength;
			this->orderedInfo = other.orderedInfo;
			this->splitInfo = other.splitInfo;
			this->sequenceNumber = other.sequenceNumber;
			this->isSplit = other.isSplit;
			
			other.pData = nullptr;
		}

		~ReliablePacket()
		{
			pData = nullptr;
		}

		void Serialize(BitStream &bitStream)
		{
			bitStream.Write(reliability);

			// Write flags / 1 Byte
			//bitStream.Write();

			if(reliability == PacketReliability::RELIABLE_ORDERED)
			{
				bitStream.Write(orderedInfo);
			}
			
			if (isSplit)
			{
				bitStream.Write(splitInfo);
			}

			bitStream.Write(dataLength);
			bitStream.Write(pData.get(), dataLength);
		}

		void Deserialize(BitStream &bitStream)
		{
			bitStream.Read(reliability);

			// Read the flags / ! Byte
			//bitStream.Read();

			if(reliability == PacketReliability::RELIABLE_ORDERED)
			{
				bitStream.Read(orderedInfo);
			}
			
			if (isSplit)
			{
				bitStream.Read(splitInfo);
			}

			bitStream.Read(dataLength);

			pData = std::unique_ptr<char>{new char[dataLength]};

			bitStream.Read(pData.get(), dataLength);
		}

		size_t GetSizeToSend()
		{
			return sizeof(reliability) + (reliability == PacketReliability::RELIABLE_ORDERED ? sizeof(orderedInfo) : 0) + sizeof(dataLength) + dataLength;
		}

		ReliablePacket & operator=(const ReliablePacket &other) = delete;

		ReliablePacket & operator=(ReliablePacket &&other)
		{

#if WIN32 /* Visual Studio stable_sort calls self move assignment operator */
			if (this == &other)
				return *this;
#endif

			this->pData = std::move(other.pData);
			this->priority = other.priority;
			this->reliability = other.reliability;
			this->dataLength = other.dataLength;
			this->orderedInfo = other.orderedInfo;
			this->splitInfo = other.splitInfo;
			this->sequenceNumber = other.sequenceNumber;
			this->isSplit = other.isSplit;

			other.pData = nullptr;
			return *this;
		}
	};

	struct DatagramPacket
	{
		DatagramHeader header;

		std::vector<ReliablePacket> packets;

		DatagramPacket()
		{

		}

		void Serialize(BitStream & bitStream)
		{

			header.Serialize(bitStream);

			if (!header.isACK && !header.isNACK)
				for (auto &packet : packets)
					packet.Serialize(bitStream);

		}

		void Deserialze(BitStream & bitStream)
		{
			auto bsSize = BYTES_TO_BITS(bitStream.Size());

			header.Deserialize(bitStream);

			packets.reserve(5);

			ReliablePacket packet;

			if (!header.isACK && !header.isNACK)
			{
				while (bitStream.ReadOffset() < bsSize)
				{
					packet.isSplit = header.isSplit;

					packet.Deserialize(bitStream);

					packets.push_back(std::move(packet));
				}
			}
			
			/*packets.shrink_to_fit();*/
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

		FlowControlHelper flowControlHelper;

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


		//std::vector<ReliablePacket> sendBuffer;
		std::array < std::vector<ReliablePacket>, static_cast<std::size_t>(PacketPriority::IMMEDIATE)> sendBuffer;

		std::vector<std::pair<std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>, std::unique_ptr<DatagramPacket>>> resendBuffer;

#if WIN32
		std::array<std::vector<ReliablePacket>, 255> orderedPacketBuffer;
#else
		std::array<std::vector<ReliablePacket>, std::numeric_limits<decltype(orderingChannel)>::max()> orderedPacketBuffer;
#endif

#if WIN32
		std::array<std::vector<ReliablePacket>, 65535> splitPacketBuffer;
#else
		std::array<std::vector<ReliablePacket>, std::numeric_limits<uint16>::max()> splitPacketBuffer;
#endif

#if WIN32
		std::array<uint16, 255> lastOrderedIndex;
#else
		std::array<uint16, std::numeric_limits<decltype(orderingChannel)>::max()> lastOrderedIndex;
#endif

	private:
		/* Methods */
		void SendACKs();


		bool ProcessPacket(InternalRecvPacket *pPacket, std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> &curTime);

		void ProcessResend(std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> &curTime);
		void ProcessOrderedPackets(std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> &curTime);
		void ProcessSend(std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> &curTime);

		bool SplitPacket(ReliablePacket & packet, DatagramPacket ** pDatagramPacket);

	public:
		ReliabilityLayer(ISocket * pSocket = nullptr);
		virtual ~ReliabilityLayer();

		void Send(char *data, size_t numberOfBitsToSend, PacketPriority priority = PacketPriority::MEDIUM, PacketReliability reliability = PacketReliability::RELIABLE);

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
		uint8 GetOrderingChannel();

		//! Sets the channel used to order sent packets
		/*!
		\param[in] ucChannel The channel on which the next ordered packets will be ordered on the remote reliability layer.
		*/
		void SetOrderingChannel(uint8 ucChannel);


		//! Gets the socket used to send packets
		/*!
		\return Socket used to send packets
		*/
		ISocket * GetSocket();

		//! Sets the socket used to send packets
		/*!
		\param[in] pSocket Socket used to send packets
		*/
		void SetSocket(ISocket * pSocket);

		
		//! Gets the remote Socket address
		/*!
		\return The remote address is the connection endpoint to which the packets will be send
		*/
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

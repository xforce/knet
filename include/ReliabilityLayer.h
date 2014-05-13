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
	enum class ReliabilityEvents : char
	{
		RECEIVE = 0,
		CONNECTION_LOST_TIMEOUT,
		HANDLE_PACKET,
		NEW_CONNECTION,
		MAX_EVENTS,
	};

	enum class PacketPriority : char
	{
		/* Will skip send buffer, so you have the control when this packet is sent*/
		IMMEDIATE = 0,
		HIGH,
		MEDIUM,
		LOW		
	};

	enum class PacketReliability : char
	{
		UNRELIABLE = 0,
		RELIABLE,
		RELIABLE_ORDERED,


		MAX
	};

	class CFlowControlHelper
	{
	private:
		int sequenceNumber = 0;

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
			if (!isACK && !isNACK)
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

	struct ReliablePacket
	{
	private:
		bool selfAllocated = false;
		char * pData = nullptr;
		unsigned short dataLength = 0;

	public:
		PacketReliability reliability = PacketReliability::UNRELIABLE;
		PacketPriority priority;

		ReliablePacket()
		{

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
			this->reliability = other.reliability;
			this->dataLength = other.dataLength;

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
			bitStream.Write(dataLength);
			bitStream.Write(pData, dataLength);
		}

		void Deserialize(CBitStream &bitStream)
		{
			bitStream.Read(reliability);
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
	};

	struct DatagramPacket
	{
		DatagramHeader header;

		/* Just hacky atm, i will make it better later */
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
		};

	private:
		ISocket * m_pSocket = nullptr;
		SocketAddress m_RemoteSocketAddress;

		CFlowControlHelper flowControlHelper;

		EventHandler<ReliabilityEvents> eventHandler;

		std::chrono::milliseconds m_msTimeout = std::chrono::milliseconds(10000);
		std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> firstUnsentAck;

		std::mutex bufferMutex;


		std::queue<InternalRecvPacket*> bufferedPacketQueue;

		

		std::vector<RemoteSystem> remoteList;

		std::vector<SequenceNumberType> acknowledgements;


		std::vector<ReliablePacket> sendBuffer;

		std::vector<std::pair<std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>, DatagramPacket*>> resendBuffer;

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


		/* Getters/Setters */


		ISocket * GetSocket();
		void SetSocket(ISocket * pSocket);

		const SocketAddress & GetRemoteAddress();
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
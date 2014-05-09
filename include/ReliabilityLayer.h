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

	struct BufferedSendPacket
	{
		char * data = nullptr;
		PacketReliability reliability;
		size_t bitLength;

		BufferedSendPacket(char * data, size_t length)
		{
			this->data = (char*)(malloc(length));
			memcpy(this->data, data, length);
			bitLength = BYTES_TO_BITS(length);
		}

		BufferedSendPacket(BufferedSendPacket &&other)
		{
			this->data = other.data;
			this->reliability = other.reliability;
			this->bitLength = other.bitLength;

			other.data = nullptr;
		}

		~BufferedSendPacket()
		{
			if (data)
				free(data);
		}
	};

	typedef int SequenceNumberType;

	class DatagramHeader
	{
	public:
		bool isACK;
		bool isNACK;
		SequenceNumberType sequenceNumber = 0;


		virtual void Serialize(CBitStream & bitStream)
		{
			bitStream.Write(isACK);
			bitStream.Write(isNACK);

			// Now fill to 1 byte to improve performance
			// This can later be used for more information in the header
			bitStream.Write0();
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
			char ch{0};
			bitStream.ReadBits(&ch, 6);

			if (!isACK && !isNACK)
				bitStream.Read(sequenceNumber);
		}

	};

	class MessageHeader : public DatagramHeader
	{

	public:
		virtual void Serialize(CBitStream & bitStream) override
		{
			DatagramHeader::Serialize(bitStream);

			// Do our stuff
		}

		virtual void Deserialize(CBitStream & bitStream) override
		{
			DatagramHeader::Deserialize(bitStream);

			// Do our stuff
		}
	};


	struct ReliablePacket
	{
	private:
		bool selfAllocated = false;

	public:
		PacketReliability reliability = PacketReliability::UNRELIABLE;

		unsigned short dataLength = 0;
		char * pData = nullptr;

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

			bitStream.Read(pData, dataLength);
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


		std::vector<BufferedSendPacket> sendBuffer;

	private:
		/* Methods */
		void SendACKs();

		
		bool ProcessPacket(InternalRecvPacket *pPacket);

	public:
		CReliabilityLayer(ISocket * pSocket = nullptr);
		virtual ~CReliabilityLayer();

		void Send(char *data, size_t numberOfBitsToSend, PacketReliability reliability);

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
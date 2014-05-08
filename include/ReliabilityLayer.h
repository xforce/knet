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
			if (sequenceNumber >= std::numeric_limits<unsigned short>::max())
				sequenceNumber = 0;

			return sequenceNumber++;
		}
	};

	class CReliabilityLayer
	{
	public:

		struct RemoteSystem
		{
			ISocket * pSocket = nullptr;
			SocketAddress address;
		};

		class DatagramHeader
		{
		public:
			bool isACK;
			bool isNACK;
			unsigned short sequenceNumber = 0xFFFF;


			virtual void Serialize(CBitStream & bitStream)
			{
				bitStream.Write(isACK);
				bitStream.Write(isNACK);

				/* To save bandwith for ack/nack */
				if (!isACK && !isNACK)
					bitStream.Write(sequenceNumber);
			}

			virtual void Deserialize(CBitStream & bitStream)
			{
				bitStream.Read(isACK);
				bitStream.Read(isNACK);

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
			MessageHeader mHeader;
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
				mHeader.Serialize(bitStream);
				bitStream.Write(reliability);
				bitStream.Write(dataLength);
				bitStream.Write(pData, dataLength);
			}

			void Deserialize(CBitStream &bitStream)
			{
				mHeader.Deserialize(bitStream);
				bitStream.Read(reliability);
				bitStream.Read(dataLength);
				
				if (pData)
					free(pData);

				pData = (char*)(malloc(dataLength));

				selfAllocated = true;

				bitStream.Read(pData, dataLength);
			}


		};

	private:
		ISocket * m_pSocket = nullptr;
		SocketAddress m_RemoteSocketAddress;

		ISocket * m_pSender = nullptr;

		std::chrono::milliseconds m_msTimeout = std::chrono::milliseconds(10000);

		std::mutex bufferMutex;
		std::queue<InternalRecvPacket*> bufferedPacketQueue;

		EventHandler<ReliabilityEvents> eventHandler;

		std::vector<RemoteSystem> remoteList;

		std::vector<unsigned short> acknowledgements;

		CFlowControlHelper flowControlHelper;


		/* Methods */
		void SendACKs();

	public:
		CReliabilityLayer(ISocket * pSocket = nullptr);
		virtual ~CReliabilityLayer();

		void Send(char *data, size_t numberOfBitsToSend, PacketReliability reliability);

		void SetTimeout(const std::chrono::milliseconds &time);
		const std::chrono::milliseconds& GetTimeout();

		void Process();

		ISocket * GetSocket();
		void SetSocket(ISocket * pSocket);

		const SocketAddress & GetRemoteAddress();
		void SetRemoteAddress(SocketAddress &socketAddress);

		InternalRecvPacket* PopBufferedPacket();

		bool ProcessPacket(InternalRecvPacket *pPacket);

		bool OnReceive(InternalRecvPacket *packet);

		decltype(eventHandler) &GetEventHandler()
		{
			return eventHandler;
		}
	};

}
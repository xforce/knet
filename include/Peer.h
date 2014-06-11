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

#include "EventHandler.h"

#include "Sockets/BerkleySocket.h"
#include "ReliabilityLayer.h"



namespace keksnl
{
	// TODO: clean up
	class Peer
	{
	private:
		keksnl::ISocket * pSocket = nullptr;

		struct System
		{
			keksnl::CReliabilityLayer reliabilityLayer;
			bool isConnected;
		};

		keksnl::CReliabilityLayer reliabilityLayer;

		std::vector<System*> remoteSystems;

		std::mutex bufferMutex;
		std::queue<keksnl::InternalRecvPacket*> bufferedPacketQueue;

	public:
		Peer();
		~Peer();

		keksnl::ISocket * GetSocket()
		{
			return pSocket;
		}

		void Start(const char *szAddress, unsigned short usPort)
		{
			keksnl::SocketBindArguments bi;

			bi.usPort = usPort;
			bi.szHostAddress = szAddress;

			pSocket->Bind(bi);
			pSocket->StartReceiving();
		}

		void Send(System &peer, const char * data, size_t len, bool im = false);

		void Connect(const char * szRemoteAddress, unsigned short usPort);

		void Process()
		{
			for (auto &system : remoteSystems)
			{
				if (system->isConnected)
				{

					// Send back
					std::string d("Send back");
					Send(*system, d.c_str(), d.size()+1, false);
					Send(*system, d.c_str(), d.size()+1, false);
					Send(*system, d.c_str(), d.size()+1, false);
				}
			}

			reliabilityLayer.Process();

			for (auto &peer : remoteSystems)
				peer->reliabilityLayer.Process();

			
		}

	private:
		/* Event handlers */
		bool OnReceive(keksnl::InternalRecvPacket* pPacket);
		bool HandleDisconnect(keksnl::SocketAddress address, keksnl::DisconnectReason reason);
		bool HandleNewConnection(keksnl::InternalRecvPacket * pPacket);
		bool HandlePacket(keksnl::ReliablePacket &packet, keksnl::SocketAddress& remoteAddress);
	
	};

};
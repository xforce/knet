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



namespace knet
{
	// TODO: clean up
	class Peer
	{
	private:
		std::shared_ptr<knet::ISocket> pSocket = nullptr;

		struct System
		{
			knet::ReliabilityLayer reliabilityLayer;
			bool isConnected;
		};

		knet::ReliabilityLayer reliabilityLayer;

		std::vector<std::shared_ptr<System>> remoteSystems;

		std::mutex bufferMutex;
		std::queue<knet::InternalRecvPacket*> bufferedPacketQueue;

		uint32 maxConnections = 5;

		bool isConnected = false;

	public:
		Peer();
		~Peer();

		std::shared_ptr<knet::ISocket> GetSocket();

		void Start(const std::string &strAddress, uint16 usPort);

		void Send(System &peer, const char * data, size_t len, bool im = false);

		void Connect(const std::string &strRemoteAddress, uint16 usPort);

		void Process();

	private:
		/* Event handlers */
		bool OnReceive(knet::InternalRecvPacket* pPacket);
		bool HandleDisconnect(knet::SocketAddress address, knet::DisconnectReason reason);
		bool HandleNewConnection(knet::InternalRecvPacket * pPacket);
		bool HandlePacket(knet::ReliablePacket &packet, knet::SocketAddress& remoteAddress);
	
	};

};
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

#include "ISocket.h"

namespace knet
{

	class BerkleySocket : public ISocket, public std::enable_shared_from_this<BerkleySocket>
	{
	private:
		SOCKET m_Socket = 0;
		SocketAddress m_BoundAddress;

		std::thread receiveThread;
		bool bRecvThreadRunning = false;
		EventHandler<SocketEvents> eventHandler;

		void RecvFromLoop();

		bool endThread = false;
	public:
		BerkleySocket();
		BerkleySocket(decltype(eventHandler) eventHandler);
		virtual ~BerkleySocket();

		virtual bool Send(const SocketAddress &remoteSystem, const char* pData, size_t length) final;
		virtual bool Bind(const SocketBindArguments &bindArgs) final;
		
		virtual const SocketType GetSocketType() const override;

		virtual void StartReceiving() final;
		virtual void StopReceiving(bool bWait = false) final;

		virtual const SocketAddress& GetSocketAddress() const final;

		virtual decltype(eventHandler) &GetEventHandler() override
		{
			return eventHandler;
		}
	};

};
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

#include "SocketAddress.h"

#include "../EventHandler.h"

#ifdef WIN32
#include <WinSock2.h>
#include <Ws2def.h>
#endif

/*
	TODO: clean this file up
*/

namespace knet
{
	/* Some helper types */
#pragma region Socket_Helper_Types
	struct SocketBindArguments
	{ 
		unsigned short usPort = 0;
		std::string szHostAddress = "";
		short addressFamily = AF_INET;
		int socketType = SOCK_DGRAM;
		int socketProtocol = 0;
	};

	enum class SocketType : uint8_t
	{
		Berkley,
	};

	enum class SocketEvents : uint8_t
	{
		RECEIVE,
		MAX_EVENTS,
	};

#define MAX_MTU_SIZE 1492
#ifdef WIN32
	using socketlen_t = int32_t;
#else
	typedef unsigned int socklen_t;
#endif
#pragma endregion

	/*
	* Interface for sockets
	*/
	class ISocket
	{
	public:
		virtual bool Send(const SocketAddress &remoteSystem, const char* pData, size_t length) = 0;
		virtual bool Bind(const SocketBindArguments &bindArgs) = 0;

		virtual const SocketType GetSocketType() const = 0;

		virtual const SocketAddress& GetSocketAddress() const = 0;

		virtual void StartReceiving() = 0;
		virtual void StopReceiving(bool bWait = false) = 0;

		virtual EventHandler<SocketEvents>& GetEventHandler() = 0;
	};


	struct InternalRecvPacket
	{
	public:
		char data[MAX_MTU_SIZE];
		size_t bytesRead;
		SocketAddress remoteAddress;
		std::chrono::steady_clock::time_point timeStamp;
		std::shared_ptr<ISocket> _socket = nullptr;

		InternalRecvPacket()
		{
			memset(data, 0, sizeof(data));
		}

		~InternalRecvPacket()
		{
			bytesRead = 0;
		}

		InternalRecvPacket(const InternalRecvPacket &other) = delete;
		InternalRecvPacket(InternalRecvPacket &&other)
		{
			memmove(this->data, other.data, sizeof(this->data));
			this->bytesRead = other.bytesRead;
			this->timeStamp = std::move(other.timeStamp);
			this->remoteAddress = std::move(other.remoteAddress);
			this->_socket = other._socket;

			other._socket = 0;
			other.bytesRead = 0;
		}

		InternalRecvPacket& operator=(InternalRecvPacket &&other)
		{
			memmove(this->data, other.data, sizeof(this->data));
			this->bytesRead = other.bytesRead;
			this->timeStamp = std::move(other.timeStamp);
			this->remoteAddress = std::move(other.remoteAddress);
			this->_socket = other._socket;

			other._socket = 0;
			other.bytesRead = 0;

			return *this;
		}
	};
};
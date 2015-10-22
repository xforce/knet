// Copyright 2015 the Crix-Dev authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#pragma once

#include "socket_address.h"

#include "../internal/event_handler.h"

#ifdef WIN32
#include <WinSock2.h>
#include <Ws2def.h>
#endif

#include <memory>
#include <cstring>
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

	static constexpr size_t MAX_MTU_SIZE = 1492;

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

		virtual internal::EventHandler<SocketEvents>& GetEventHandler() = 0;
	};


	struct InternalRecvPacket
	{
	public:
		char data[MAX_MTU_SIZE];
		size_t bytesRead;
		SocketAddress remoteAddress;
		std::chrono::steady_clock::time_point timeStamp;
		std::weak_ptr<ISocket> _socket;

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

			other.bytesRead = 0;
		}

		InternalRecvPacket& operator=(InternalRecvPacket &&other)
		{
			memmove(this->data, other.data, sizeof(this->data));
			this->bytesRead = other.bytesRead;
			this->timeStamp = std::move(other.timeStamp);
			this->remoteAddress = std::move(other.remoteAddress);
			this->_socket = other._socket;

			other.bytesRead = 0;

			return *this;
		}
	};
};

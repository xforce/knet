// Copyright 2015 the Crix-Dev authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "isocket.h"

#include "../internal/event_handler.h"

#ifdef WIN32
#include <WinSock2.h>
#include <Windows.h>
#endif

#include <thread>

namespace knet
{

	class BerkleySocket : public ISocket, public std::enable_shared_from_this<BerkleySocket>
	{
	private:
		int m_Socket = 0;
		SocketAddress m_BoundAddress;

		std::thread receiveThread;
		bool bRecvThreadRunning = false;
		internal::EventHandler<SocketEvents> eventHandler;

		void RecvFromLoop();

		bool endThread = false;
	public:
		BerkleySocket();
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

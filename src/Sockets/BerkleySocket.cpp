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

#include <Sockets/BerkleySocket.h>

#include <thread>

namespace keksnl
{
#ifdef WIN32
	WSADATA wsaData;
#endif

	CBerkleySocket::CBerkleySocket()
	{
// NOTE: this will later be moved so dont care atm
#ifdef WIN32
		WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
	}

	CBerkleySocket::~CBerkleySocket()
	{
		endThread = true;
#ifdef WIN32
		WSACleanup();
#endif
	}

	bool CBerkleySocket::Send(const SocketAddress &remoteSystem, const char* pData, size_t length)
	{
		int sentLen = 0;
		do
		{
			sentLen = sendto(m_Socket, pData, length, 0, (const sockaddr*)&remoteSystem.address.addr4, sizeof(remoteSystem.address.addr4));
			if (sentLen <= -1)
			{
#ifdef WIN32
				int errCode = WSAGetLastError();

				LPSTR errString = NULL;

				int size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0, errCode, 0, (LPSTR)&errString, 0, 0);

				DEBUG_LOG("\nError code %d:  %s\n\n", errCode, errString);

				LocalFree(errString); // if you don't do this, you will get a memory leak
#endif
			}
		}
		while (sentLen == 0);

		return true;
	}

	bool CBerkleySocket::Bind(const SocketBindArguments &bindArgs)
	{
		// TODO: make IPv6


		memset(&m_BoundAddress.address.addr4, 0, sizeof(sockaddr_in));
		m_BoundAddress.address.addr4.sin_port = htons(bindArgs.usPort);

		if (bindArgs.szHostAddress)
		{
			m_BoundAddress.address.addr4.sin_addr.s_addr = inet_addr(bindArgs.szHostAddress);
		}
		else
		{
			m_BoundAddress.address.addr4.sin_addr.s_addr = INADDR_ANY;
		}

		m_BoundAddress.address.addr4.sin_family = bindArgs.addressFamily;

		// Setup the socket
		m_Socket = socket(bindArgs.addressFamily, bindArgs.socketType, bindArgs.socketProtocol);


		// TODO: set initial socket options

		int sock_opt = 1024 * 128; // 128 KB
		setsockopt( m_Socket, SOL_SOCKET, SO_RCVBUF, (const char*)&sock_opt, sizeof(sock_opt));

		sock_opt = 1024 * 16; // 16 KB
		setsockopt(m_Socket, SOL_SOCKET, SO_SNDBUF, (const char*)&sock_opt, sizeof(sock_opt));

#if 0 // Actually this makes it slower, because of CPU Bound crap fucking threads are taking to much cpu - I will work on that later
		u_long iMode = 1;
		ioctlsocket(m_Socket, FIONBIO, &iMode);
#endif

		// Bind the socket to the address in bindArgs
		int ret = bind(m_Socket, (struct sockaddr *)&m_BoundAddress.address.addr4, sizeof(m_BoundAddress.address.addr4));

		if (ret <= -1)
		{
#ifdef WIN32
			int errCode = WSAGetLastError();

			LPSTR errString = NULL;

			int size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0, errCode, 0, (LPSTR)&errString, 0, 0);

			DEBUG_LOG("Error code %d:  %s\n\n", errCode, errString);

			LocalFree(errString); // if you don't do this, you will get an
#endif
			return false;
		}

		sockaddr_in sa;
		memset(&sa, 0, sizeof(sockaddr_in));
		socklen_t len = sizeof(sa);

		getsockname(m_Socket, (sockaddr*)&sa, &len);
		m_BoundAddress.address.addr4.sin_port = sa.sin_port;
		m_BoundAddress.address.addr4.sin_addr.s_addr = sa.sin_addr.s_addr;

		if (m_BoundAddress.address.addr4.sin_addr.s_addr == INADDR_ANY)
		{
			m_BoundAddress.address.addr4.sin_addr.s_addr = inet_addr("127.0.0.1");
		}

		return true;
	}

	const SocketType CBerkleySocket::GetSocketType() const
	{

		return SocketType::Berkley;
	}

	void CBerkleySocket::RecvFromLoop()
	{
		InternalRecvPacket * packet = new InternalRecvPacket();

		while (endThread == false)
		{
			const auto recvFromBlocking = [](SOCKET _socket, InternalRecvPacket &packet) -> bool
			{

				sockaddr_in sa;
				socklen_t sockLen = sizeof(sa);

				memset(&sa, 0, sizeof(sockaddr_in));
				const int flag = 0;

				sa.sin_family = AF_INET;
				sa.sin_port = 0;

				int recvLen = recvfrom(_socket, packet.data, sizeof(packet.data), flag, (sockaddr*)&sa, (socklen_t*)&sockLen);

				if (recvLen <= 0)
				{
					// TODO: output detailed error
					return false;
				}

				packet.remoteAddress.address.addr4.sin_family = sa.sin_family;
				packet.remoteAddress.address.addr4.sin_port = sa.sin_port;
				packet.remoteAddress.address.addr4.sin_addr.s_addr = sa.sin_addr.s_addr;
				packet.bytesRead = recvLen;
				packet.timeStamp = std::chrono::steady_clock::now();

				return true;
			};



			// TODO: handle return
			if (recvFromBlocking(m_Socket, *packet))
			{
				packet->pSocket = this;

				if (eventHandler)
				{
					eventHandler.Call(SocketEvents::RECEIVE, packet);
				}
				else
					delete packet;

				packet = new InternalRecvPacket();
			}
			else
				std::this_thread::yield();

		}

		bRecvThreadRunning = false;
	}

	void CBerkleySocket::StartReceiving()
	{
		if (bRecvThreadRunning)
			return;

		endThread = false;

		std::thread rTh(&CBerkleySocket::RecvFromLoop, this);
		receiveThread = std::move(rTh);
		receiveThread.detach();

		bRecvThreadRunning = true;
	}

	void CBerkleySocket::StopReceiving(bool bWait)
	{
		endThread = true;

		// Send a package to ourself so the receive loop is processes after endThread was set to true
		unsigned long zero = 0;
		Send(m_BoundAddress, (char*)&zero, 4);

		if (bWait)
		{
			while (bRecvThreadRunning)
			{
				Send(m_BoundAddress, (char*)&zero, 4);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}
	}

	const SocketAddress& CBerkleySocket::GetSocketAddress() const
	{
		return m_BoundAddress;
	}
};

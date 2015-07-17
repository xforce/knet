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

#ifndef WIN32
#include <fcntl.h>
#else
#include <ws2tcpip.h>
#endif

namespace knet
{
#ifdef WIN32
	WSADATA wsaData;
#endif

	BerkleySocket::BerkleySocket()
	{
// NOTE: this will later be moved so dont care atm
#ifdef WIN32
		WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
	}

	BerkleySocket::~BerkleySocket()
	{
		endThread = true;
#ifdef WIN32
		WSACleanup();
#endif
	}

	bool BerkleySocket::Send(const SocketAddress &remoteSystem, const char* pData, size_t length)
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

				FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0, errCode, 0, (LPSTR)&errString, 0, 0);

				DEBUG_LOG("\nError code %d:  %s\n\n", errCode, errString);

				LocalFree(errString); // if you don't do this, you will get a memory leak
#endif
			}
		}
		while (sentLen == 0);

		return true;
	}

	bool BerkleySocket::Bind(const SocketBindArguments &bindArgs)
	{
		// TODO: add IPv6 support


		memset(&m_BoundAddress.address.addr4, 0, sizeof(sockaddr_in));
		m_BoundAddress.address.addr4.sin_port = htons(bindArgs.usPort);

		if (!bindArgs.szHostAddress.empty())
		{
			m_BoundAddress.address.addr4.sin_addr.s_addr = inet_addr(bindArgs.szHostAddress.c_str());
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
#ifdef WIN32
	u_long iMode = 1;
	ioctlsocket(m_Socket, FIONBIO, &iMode);
#else
	int flags = fcntl(m_Socket, F_GETFL, 0);
	flags = (flags|O_NONBLOCK);
	fcntl(m_Socket, F_SETFL, flags);
#endif
#endif

		// Bind the socket to the address in bindArgs
		int ret = bind(m_Socket, (struct sockaddr *)&m_BoundAddress.address.addr4, sizeof(m_BoundAddress.address.addr4));

		if (ret <= -1)
		{
#ifdef WIN32
			int errCode = WSAGetLastError();

			LPSTR errString = NULL;

			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0, errCode, 0, (LPSTR)&errString, 0, 0);

			DEBUG_LOG("Error code %d:  %s in [%s]\n\n", errCode, errString, __FUNCSIG__);

			LocalFree(errString);
#elif (defined(__GNUC__) || defined(__GCCXML__) ) && !defined(_WIN32)
			closesocket__(rns2Socket);
			switch (ret)
			{
			case EBADF:
				DEBUG_LOG("bind(): sockfd is not a valid descriptor.\n"); break;
			case ENOTSOCK:
				DEBUG_LOG("bind(): Argument is a descriptor for a file, not a socket.\n"); break;
			case EINVAL:
				DEBUG_LOG("bind(): The addrlen is wrong, or the socket was not in the AF_UNIX family.\n"); break;
			case EROFS:
				DEBUG_LOG("bind(): The socket inode would reside on a read-only file system.\n"); break;
			case EFAULT:
				DEBUG_LOG("bind(): my_addr points outside the user's accessible address space.\n"); break;
			case ENAMETOOLONG:
				DEBUG_LOG("bind(): my_addr is too long.\n"); break;
			case ENOENT:
				DEBUG_LOG("bind(): The file does not exist.\n"); break;
			case ENOMEM:
				DEBUG_LOG("bind(): Insufficient kernel memory was available.\n"); break;
			case ENOTDIR:
				DEBUG_LOG("bind(): A component of the path prefix is not a directory.\n"); break;
			case EACCES:
				DEBUG_LOG("bind(): Search permission is denied on a component of the path prefix.\n"); break;

			case ELOOP:
				DEBUG_LOG("bind(): Too many symbolic links were encountered in resolving my_addr.\n"); break;

			default:
				DEBUG_LOG("Unknown bind() error %i.\n", ret); break;
			}
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

	const SocketType BerkleySocket::GetSocketType() const
	{
		return SocketType::Berkley;
	}

	void BerkleySocket::RecvFromLoop()
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
#ifdef WIN32
					int errCode = WSAGetLastError();

					LPSTR errString = NULL;

					FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0, errCode, 0, (LPSTR) &errString, 0, 0);

					DEBUG_LOG("Error code %d:  %s in [%s]\n\n", errCode, errString, __FUNCSIG__);

					LocalFree(errString);
#endif
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
				packet->_socket = shared_from_this();

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

	void BerkleySocket::StartReceiving()
	{
		if (bRecvThreadRunning)
			return;

		endThread = false;

		std::thread rTh(&BerkleySocket::RecvFromLoop, this);
		receiveThread = std::move(rTh);
		receiveThread.detach();

		bRecvThreadRunning = true;
	}

	void BerkleySocket::StopReceiving(bool bWait)
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

	const SocketAddress& BerkleySocket::GetSocketAddress() const
	{
		return m_BoundAddress;
	}
};

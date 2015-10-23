// Copyright 2015 the kNet authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <netinet/in.h>
#endif

namespace knet
{
	class SocketAddress
	{
	public:
		union// In6OrIn4
		{
#if _IPV6==1
			struct sockaddr_storage sa_stor;
			struct sockaddr_in6 addr6;
#endif

			struct sockaddr_in addr4;
		} address;

		bool operator== (const SocketAddress &cP1) const
		{
			return (address.addr4.sin_addr.s_addr == cP1.address.addr4.sin_addr.s_addr
					&& address.addr4.sin_port == cP1.address.addr4.sin_port);
		}
	};
};

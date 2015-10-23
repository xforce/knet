// Copyright 2015 the kNet authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace net
{
	// Some helper types to better wrap the underlying network library
	struct NetworkConnectInfo
	{
		std::string strHost;
		uint16_t port;
		std::string strPassword;
	};

	struct NetworkEndPointInformation
	{
		std::string strHost;
		uint16_t port;
	};

	struct NetworkInformation
	{
		int maxConnections;
		std::string strPassword;
		std::vector<NetworkEndPointInformation> localEndPoints;
		bool incoming = false;
	};

	enum class PacketPriority : uint8_t
	{
		LOW,
		MEDIUM,
		HIGH,
		IMMEDIATE /* skip send buffer */,
		MAX,
	};

	enum class PacketReliability : uint8_t
	{
		UNRELIABLE = 0,
		UNRELIABLE_SEQUENCED,
		RELIABLE,
		RELIABLE_ORDERED,
		RELIABLE_SEQUENCED
	};

	struct NetworkAddress
	{
		std::string address; /* This is used internally to translate to a unique specifier to identify the system */

		bool IsUnassigned()
		{
			return address == "unassigned";
		}

		bool operator ==(const NetworkAddress &other) const
		{
			return other.address == address;
		}
	};

	static const NetworkAddress UNASSIGNED_ADDRESS;

	enum class NetworkEvents : uint8_t
	{
		ConnectionSucceeded,
		ConnectionFailed, /* With reason */
		ConnectionAttemptFailed,
		Disconnected, /* With reason */
		NewConnection, /* */
		OnPacket,
	};

	enum class ConnectionFailedReason : uint8_t
	{
		AlreadyConnected,
		Banned,
		InvalidPassword,
		ServerFull,
		AttemptFailed,
		IncompatibleProtocol,
	};

	enum class ConnectionAttemptFailedReason : uint8_t
	{
		FailedToResolveDomainName,
		AlreadyConnected,
		AlreadyConnecting,
		Other,
	};

	enum class ClosedConnectionReason : uint8_t
	{
		ClosedByUser,
		ConnectionLost,
		DisconnectNotification,
	};
}

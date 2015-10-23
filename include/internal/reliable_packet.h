// Copyright 2015 the kNet authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "datagram_header.h"
#include "sockets/socket_address.h"

namespace knet
{

	enum PacketPriority : uint8_t
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
		RELIABLE_SEQUENCED,

		MAX,
	};

	using OrderedIndexType = uint16_t;
	using OrderedChannelType = uint8_t;

	struct OrderedInfo
	{
		OrderedIndexType index;
		OrderedChannelType channel;
	};

	struct SplitInfo
	{
		uint16_t index;
		uint16_t packetIndex;
		uint8_t isEnd = false;
	};

	using SequenceIndexType = uint16_t;

	struct SequenceInfo
	{
		SequenceIndexType index;
		OrderedChannelType channel;
	};

	struct ReliablePacket
	{
	private:
		std::unique_ptr<char> _data = nullptr;
		uint16_t _dataLength = 0;

	public:
		PacketReliability reliability = PacketReliability::UNRELIABLE;
		PacketPriority priority;

		OrderedInfo orderedInfo;

		SplitInfo splitInfo;

		SequenceInfo sequenceInfo;

		SequenceNumberType sequenceNumber = 0;
		SocketAddress socketAddress;

		bool isSplit = false;

		ReliablePacket() = default;

		const char * Data()
		{
			return _data.get();
		}

		decltype(_dataLength) Size()
		{
			return _dataLength;
		}

		ReliablePacket(const char * data, ::size_t length)
		{

			_data = std::unique_ptr<char>{new char[length]};
			memcpy(_data.get(), data, length);
			_dataLength = static_cast<uint16_t>(length);
		}

		ReliablePacket(ReliablePacket &&other)
		{
#if _WIN32 /* Visual Studio stable_sort calls self move assignment operator */
			if (this == &other)
				return;
#endif

			this->_data = std::move(other._data);
			this->priority = other.priority;
			this->reliability = other.reliability;
			this->_dataLength = other._dataLength;
			this->orderedInfo = other.orderedInfo;
			this->splitInfo = other.splitInfo;
			this->sequenceNumber = other.sequenceNumber;
			this->isSplit = other.isSplit;

			other._data = nullptr;
		}

		~ReliablePacket()
		{
			_data = nullptr;
		}

		void Serialize(BitStream &bitStream)
		{
			bitStream.Write(reliability);

			if (reliability == PacketReliability::RELIABLE_ORDERED)
			{
				bitStream.Write(orderedInfo);
			}
			else if (reliability == PacketReliability::RELIABLE_SEQUENCED
				|| reliability == PacketReliability::UNRELIABLE_SEQUENCED)
			{
				bitStream.Write(sequenceInfo);
			}

			if (isSplit)
			{
				bitStream.Write(splitInfo);
			}

			bitStream.Write(_dataLength);
			bitStream.Write(_data.get(), _dataLength);
		}

		void Deserialize(BitStream &bitStream)
		{
			bitStream.Read(reliability);

			if (reliability == PacketReliability::RELIABLE_ORDERED)
			{
				bitStream.Read(orderedInfo);
			}
			else if (reliability == PacketReliability::RELIABLE_SEQUENCED
				|| reliability == PacketReliability::UNRELIABLE_SEQUENCED)
			{
				bitStream.Read(sequenceInfo);
			}

			if (isSplit)
			{
				bitStream.Read(splitInfo);
			}

			bitStream.Read(_dataLength);

			_data = std::unique_ptr<char>{new char[_dataLength]};

			bitStream.Read(_data.get(), _dataLength);
		}

		size_t GetSizeToSend()
		{
			return sizeof(reliability)
				+ (reliability == PacketReliability::RELIABLE_ORDERED ? sizeof(orderedInfo) : 0)
				+ (reliability == PacketReliability::RELIABLE_SEQUENCED ? sizeof(sequenceInfo) : 0)
				+ (reliability == PacketReliability::UNRELIABLE_SEQUENCED ? sizeof(sequenceInfo) : 0)
				+ sizeof(_dataLength) + _dataLength;
		}

		ReliablePacket & operator=(ReliablePacket &&other)
		{

#if _WIN32 /* Visual Studio stable_sort calls self move assignment operator */
			if (this == &other)
				return *this;
#endif

			this->_data = std::move(other._data);
			this->priority = other.priority;
			this->reliability = other.reliability;
			this->_dataLength = other._dataLength;
			this->orderedInfo = other.orderedInfo;
			this->splitInfo = other.splitInfo;
			this->sequenceNumber = other.sequenceNumber;
			this->isSplit = other.isSplit;

			other._data = nullptr;
			return *this;
		}
	};
};

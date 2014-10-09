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
#include "DatagramHeader.h"
#include "Sockets/SocketAddress.h"

namespace knet
{

	enum PacketPriority : uint8
	{
		LOW,
		MEDIUM,
		HIGH,
		IMMEDIATE /* skip send buffer */,
		MAX,
	};

	enum class PacketReliability : uint8
	{
		UNRELIABLE = 0,
		UNRELIABLE_SEQUENCED,

		RELIABLE,
		RELIABLE_ORDERED,
		RELIABLE_SEQUENCED,

		MAX,
	};

	typedef uint16 OrderedIndexType;

	struct OrderedInfo
	{
		OrderedIndexType index;
		uint8 channel;
	};

	struct SplitInfo
	{
		uint16 index;
		uint16 packetIndex;
		uint8 isEnd = false;
	};

	typedef uint16 SequenceIndexType;

	struct SequenceInfo
	{
		SequenceIndexType index;
		uint8 channel;
	};

	struct ReliablePacket
	{
	private:
		std::unique_ptr<char> pData = nullptr;
		uint16 dataLength = 0;

	public:
		PacketReliability reliability = PacketReliability::UNRELIABLE;
		PacketPriority priority;

		OrderedInfo orderedInfo;

		SplitInfo splitInfo;

		SequenceInfo sequenceInfo;

		SequenceNumberType sequenceNumber = 0;
		SocketAddress socketAddress;

		bool isSplit = false;

		ReliablePacket()
		{

		}

		ReliablePacket(const ReliablePacket &other) = delete;

		const char * Data()
		{
			return pData.get();
		}

		decltype(dataLength) Size()
		{
			return dataLength;
		}

		ReliablePacket(const char * data, ::size_t length)
		{

			pData = std::unique_ptr<char>{new char[length]};
			memcpy(pData.get(), data, length);
			dataLength = static_cast<uint16>(length);
		}

		ReliablePacket(ReliablePacket &&other)
		{
#if WIN32 /* Visual Studio stable_sort calls self move assignment operator */
			if (this == &other)
				return;
#endif

			this->pData = std::move(other.pData);
			this->priority = other.priority;
			this->reliability = other.reliability;
			this->dataLength = other.dataLength;
			this->orderedInfo = other.orderedInfo;
			this->splitInfo = other.splitInfo;
			this->sequenceNumber = other.sequenceNumber;
			this->isSplit = other.isSplit;

			other.pData = nullptr;
		}

		~ReliablePacket()
		{
			pData = nullptr;
		}

		void Serialize(BitStream &bitStream)
		{
			bitStream.Write(reliability);

			// Write flags / 1 Byte
			//bitStream.Write();

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

			bitStream.Write(dataLength);
			bitStream.Write(pData.get(), dataLength);
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

			bitStream.Read(dataLength);

			pData = std::unique_ptr<char>{new char[dataLength]};

			bitStream.Read(pData.get(), dataLength);
		}

		size_t GetSizeToSend()
		{
			return sizeof(reliability)
				+ (reliability == PacketReliability::RELIABLE_ORDERED ? sizeof(orderedInfo) : 0)
				+ (reliability == PacketReliability::RELIABLE_SEQUENCED ? sizeof(sequenceInfo) : 0)
				+ (reliability == PacketReliability::UNRELIABLE_SEQUENCED ? sizeof(sequenceInfo) : 0)
				+ sizeof(dataLength) + dataLength;
		}

		// Packets can not be copied
		ReliablePacket & operator=(const ReliablePacket &other) = delete;

		ReliablePacket & operator=(ReliablePacket &&other)
		{

#if WIN32 /* Visual Studio stable_sort calls self move assignment operator */
			if (this == &other)
				return *this;
#endif

			this->pData = std::move(other.pData);
			this->priority = other.priority;
			this->reliability = other.reliability;
			this->dataLength = other.dataLength;
			this->orderedInfo = other.orderedInfo;
			this->splitInfo = other.splitInfo;
			this->sequenceNumber = other.sequenceNumber;
			this->isSplit = other.isSplit;

			other.pData = nullptr;
			return *this;
		}
	};
};
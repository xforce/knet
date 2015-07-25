#pragma once

#include "Common.h"
#include "DatagramHeader.h"
#include "ReliablePacket.h"

#include <vector>

namespace knet
{
	struct DatagramPacket
	{
		DatagramHeader header;

		std::vector<ReliablePacket> packets;

		DatagramPacket & operator=(const DatagramPacket &other) = delete;
		DatagramPacket(const DatagramPacket &other) = delete;

		DatagramPacket()
		{

		}

		void Serialize(BitStream & bitStream)
		{

			header.Serialize(bitStream);

			if (!header.isACK && !header.isNACK)
				for (auto &packet : packets)
					packet.Serialize(bitStream);

		}

		void Deserialze(BitStream & bitStream)
		{
			auto bsSize = BytesToBits(bitStream.Size());

			header.Deserialize(bitStream);

			packets.reserve(5);

			ReliablePacket packet;

			if (!header.isACK && !header.isNACK)
			{
				while (bitStream.ReadOffset() < bsSize)
				{
					packet.isSplit = header.isSplit;

					packet.Deserialize(bitStream);

					packets.push_back(std::move(packet));
				}
			}
		}

		size_t GetSizeToSend()
		{
			size_t size = 0;
			for (auto &packet : packets)
			{
				size += packet.GetSizeToSend();
			}

			return size + header.GetSizeToSend();
		}
	};
}
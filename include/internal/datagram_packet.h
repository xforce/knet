// Copyright 2015 the Crix-Dev authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "datagram_header.h"
#include "reliable_packet.h"

#include "helper_types.h"

#include <vector>

namespace knet
{
	struct DatagramPacket : public internal::non_copyable
	{
		DatagramHeader header;

		std::vector<ReliablePacket> packets;

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

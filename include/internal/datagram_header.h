// Copyright 2015 the kNet authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "bitstream.h"

#include <cstdint>

namespace knet
{

	using SequenceNumberType = int32_t;

	class DatagramHeader
	{
	public:
		bool isACK;
		bool isNACK;
		bool isReliable = true;
		bool isSplit = false;
		SequenceNumberType sequenceNumber = 0;


		virtual void Serialize(BitStream & bitStream)
		{
			bitStream.Write(isACK);
			bitStream.Write(isNACK);
			bitStream.Write(isReliable);
			bitStream.Write(isSplit);

			// Now fill to 1 byte to improve performance
			// This can later be used for more information in the header
			bitStream.AlignWriteToByteBoundary();

			/* To save bandwith for ack/nack */
			if (!isACK && !isNACK && isReliable)
				bitStream.Write(sequenceNumber);
		}

		virtual void Deserialize(BitStream & bitStream)
		{
			bitStream.Read(isACK);
			bitStream.Read(isNACK);
			bitStream.Read(isReliable);
			bitStream.Read(isSplit);

			bitStream.AlignReadToByteBoundary();

			if (!isACK && !isNACK && isReliable)
				bitStream.Read(sequenceNumber);
		}

		size_t GetSizeToSend()
		{
			// Meh hardcoded size
			return 1 + ((!isACK && !isNACK && isReliable) ? sizeof(sequenceNumber) : 0);
		}
	};

};

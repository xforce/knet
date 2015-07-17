/*
* Copyright (C) 2014-2015 Crix-Dev
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
#include "BitStream.h"

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
			bitStream.AddWriteOffset(4);

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

			bitStream.SetReadOffset(bitStream.ReadOffset() + 4);

			if (!isACK && !isNACK && isReliable)
				bitStream.Read(sequenceNumber);
		}

		size_t GetSizeToSend()
		{
			return 1 + ((!isACK && !isNACK && isReliable) ? sizeof(sequenceNumber) : 0);
		}
	};

};
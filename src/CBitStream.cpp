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

#include "CBitStream.h"

namespace keksnl
{

	CBitStream::CBitStream()
		: CBitStream(10)
	{
	}
	
	CBitStream::CBitStream(size_t initialBytes)
	{
		if (initialBytes < BITSTREAM_STACK_SIZE)
		{
			pData = stackData;
			bitsAllocated = BYTES_TO_BITS(BITSTREAM_STACK_SIZE);
		}
		else
		{
			// TODO: track avg to make allocation to better fit the need
			pData = (decltype(pData))malloc(initialBytes);
			bitsAllocated = BYTES_TO_BITS(initialBytes);
		}
	}

	CBitStream::CBitStream(CBitStream &&other)
	{
		if (pData != stackData)
			if (pData)
				free(pData);

		if (other.pData == other.stackData)
			memcpy(this->stackData, other.stackData, BITSTREAM_STACK_SIZE);
		else
			pData = other.pData;

		bitsAllocated = other.bitsAllocated;
		bitsUsed = other.bitsUsed;
		readOffset = other.readOffset;

		other.bitsUsed = 0;
		other.bitsAllocated = 0;
		other.pData = nullptr;
		other.readOffset = 0;
	}

	CBitStream::CBitStream(unsigned char* _data, const int lengthInBytes, bool _copyData)
	{
		bitsUsed = BYTES_TO_BITS(lengthInBytes);
		readOffset = 0;
		//copyData = _copyData;
		bitsAllocated = BYTES_TO_BITS(lengthInBytes);

		if (_copyData || 1==1)
		{
			if (lengthInBytes < BITSTREAM_STACK_SIZE)
			{
				pData = stackData;
				bitsAllocated = BYTES_TO_BITS(BITSTREAM_STACK_SIZE);
			}
			else
			//if (lengthInBytes > 0)
			{
				pData = (unsigned char*)malloc((size_t)lengthInBytes);
				
				assert(pData);

				
			}
			/*else
				pData = 0;*/

			memcpy(pData, _data, (size_t)lengthInBytes);
		}
		else
			pData = (unsigned char*)_data;
	}

	CBitStream::~CBitStream()
	{
		if (pData != stackData)
			if (pData)
				free(pData);

		bitsAllocated = 0;
		bitsUsed = 0;
		readOffset = 0;
	}

	bool CBitStream::AllocateBits(size_t numberOfBits)
	{
		size_t newBitsAllocated = bitsAllocated + numberOfBits;

		if (BITS_TO_BYTES(newBitsAllocated) < BITSTREAM_STACK_SIZE)
		{
			return true;
		}

		auto data = (decltype(pData))realloc(pData, BITS_TO_BYTES(newBitsAllocated));

		if (!data)
			return false;

		if (pData == stackData)
			memcpy(data, stackData, BITS_TO_BYTES(bitsAllocated));

		pData = data;

		assert(pData);

		bitsAllocated = newBitsAllocated;

		return true;
	}

	bool CBitStream::PrepareWrite(size_t bitsToWrite)
	{
		if ((bitsAllocated - bitsUsed) >= bitsToWrite)
			return true;
		else
		{
			return AllocateBits(bitsToWrite);
		}
	}

	void CBitStream::Write0()
	{
		PrepareWrite(1);

		// New bytes need to be zeroed
		if ((bitsUsed & 7) == 0)
			pData[bitsUsed >> 3] = 0;

		++bitsUsed;
	}

	void CBitStream::Write1()
	{
		PrepareWrite(1);

		size_t numberOfBitsMod8 = bitsUsed & 7;

		if (numberOfBitsMod8 == 0)
			pData[bitsUsed >> 3] = 0x80;
		else
			pData[bitsUsed >> 3] |= 0x80 >> (numberOfBitsMod8); // Set the bit to 1

		++bitsUsed;
	}

	bool CBitStream::Write(const unsigned char *data, size_t size)
	{
		PrepareWrite(BYTES_TO_BITS(size));

		const size_t bitsToWrite = BYTES_TO_BITS(size);

		if ((bitsUsed & 7) == 0)
		{
			memcpy(this->pData + BITS_TO_BYTES(bitsUsed), data, size);
			bitsUsed += bitsToWrite;
			return true;
		}
		else
		{
			return WriteBits(data, bitsToWrite);
		}
	}

	bool CBitStream::Write(const char *pData, size_t size)
	{
		PrepareWrite(BYTES_TO_BITS(size));

		// This might cause bad performance because we have todo the same in writebits. Question How can we optimize that?
		// Check if we can just use memcpy :)
		const size_t bitsToWrite = BYTES_TO_BITS(size);

		if ((bitsUsed & 7) == 0)
		{
			memcpy(this->pData + BITS_TO_BYTES(bitsUsed), pData, size);
			bitsUsed += bitsToWrite;
			return true;
		}
		else
		{
			return WriteBits((unsigned char*)pData, bitsToWrite);
		}
	}


	bool CBitStream::WriteBits(const unsigned char *pData, size_t  numberOfBits)
	{
		const size_t numberOfBitsUsedMod8 = bitsUsed & 7;

		if (numberOfBitsUsedMod8 == 0)
		{
			// Just do a memcpy instead of do this in the loop below
			memcpy(this->pData + BITS_TO_BYTES(bitsUsed), pData, BITS_TO_BYTES(numberOfBits));
			bitsUsed += numberOfBits;

			return true;
		}

		unsigned char dataByte;
		const baseType * inputPtr = pData;

		while (numberOfBits > 0)
		{
			dataByte = *(inputPtr++);

			// Copy over the new data.
			*(this->pData + (numberOfBits >> 3)) |= dataByte >> (numberOfBitsUsedMod8);

			if (8 - (numberOfBitsUsedMod8) < 8 && 8 - (numberOfBitsUsedMod8) < numberOfBits)
			{
				*(this->pData + (bitsUsed >> 3) + 1) = (unsigned char)(dataByte << (8 - (numberOfBitsUsedMod8)));
			}

			if (numberOfBits >= 8)
			{
				bitsUsed += 8;
				numberOfBits -= 8;
			}
			else
			{
				bitsUsed += numberOfBits;
				return true;
			}
		}

		return true;
	}


	bool CBitStream::Read(char *pData, size_t size)
	{
		if ((readOffset & 7) == 0)
		{
			memcpy(pData, this->pData + (readOffset >> 3), (size_t)size);

			readOffset += size << 3;
		}
		else
		{
			// Read dat bits
			ReadBits(pData, BYTES_TO_BITS(size));
		}

		return true;
	}

	bool CBitStream::ReadBits(char *pData, size_t  numberOfBits)
	{
		if (numberOfBits <= 0)
			return false;

		if (readOffset + numberOfBits > bitsUsed)
			return false;


		const size_t readOffsetMod8 = readOffset & 7;


		if (readOffsetMod8 == 0 && (numberOfBits & 7) == 0)
		{
			memcpy(pData, this->pData + (readOffset >> 3), numberOfBits >> 3);
			readOffset += numberOfBits;
			return true;
		}
		else
		{
			size_t offset = 0;

			memset(pData, 0, (size_t)BITS_TO_BYTES(numberOfBits));

			while (numberOfBits > 0)
			{
				*(pData + offset) |= *(this->pData + (readOffset >> 3)) << (readOffsetMod8);

				if (readOffsetMod8 > 0 && numberOfBits > 8 - (readOffsetMod8))
					*(pData + offset) |= *(this->pData + (readOffset >> 3) + 1) >> (8 - (readOffsetMod8));

				if (numberOfBits >= 8)
				{
					numberOfBits -= 8;
					readOffset += 8;
					offset++;
				}
				else
				{
					readOffset += 8 + (numberOfBits - 8);
					return true;
				}
			}

			return true;
		}
	}

	bool CBitStream::Write(const std::string &str)
	{
		return (Write(str.length())
				&& Write((unsigned char*)str.c_str(), str.size()));
	}


	void CBitStream::SetReadOffset(size_t readOffset)
	{
		this->readOffset = readOffset;
	}

	void CBitStream::SetWriteOffset(size_t writeOffset)
	{ 
		PrepareWrite(writeOffset - this->bitsUsed);

		bitsUsed = writeOffset;
	}

	void CBitStream::AddWriteOffset(size_t writeOffset)
	{ 
		PrepareWrite(this->bitsUsed + writeOffset);

		bitsUsed += writeOffset;
	}


#if 0
	template<typename T>
	bool CBitStream::Write(const T &value)
	{
		return Write((unsigned char*)&value, sizeof(T));
	}

	template<typename T>
	bool CBitStream::Read(T &value)
	{
		return Read((unsigned char*)&value, sizeof(T));
	}
#endif

#pragma region operators

	bool CBitStream::operator=(const CBitStream &right)
	{
		PrepareWrite(right.bitsUsed);

		memcpy(this->pData, right.pData, BITS_TO_BYTES(right.bitsUsed));
		this->bitsUsed = right.bitsUsed;
		this->readOffset = right.readOffset;

		return true;
	}

	bool CBitStream::operator=(CBitStream &&right)
	{
		if (pData != stackData)
			if (pData)
				free(pData);

		if (right.pData == right.stackData)
			memcpy(this->stackData, right.stackData, BITSTREAM_STACK_SIZE);
		else
			pData = right.pData;

		bitsAllocated = right.bitsAllocated;
		bitsUsed = right.bitsUsed;
		readOffset = right.readOffset;

		right.bitsUsed = 0;
		right.bitsAllocated = 0;
		right.pData = nullptr;
		right.readOffset = 0;

		return true;
	}

#pragma endregion
}

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

namespace knet
{
	template<typename T>
	inline constexpr T BitsToBytes(T x)
	{
		return (((x)+7) >> 3);
	}

	template<typename T>
	inline constexpr T BytesToBits(T x)
	{
		return ((x) << 3);
	}

#define BITSTREAM_STACK_SIZE 256

	class BitStream
	{
	public:
		using baseType = unsigned char;


	private:
		baseType* pData = nullptr;
		size_t readOffset = 0;
		size_t bitsAllocated = 0;
		size_t bitsUsed = 0;

		baseType stackData[BITSTREAM_STACK_SIZE];


	public:
		BitStream() noexcept;

		BitStream(size_t initialBytes) noexcept;

		BitStream(BitStream &&other) noexcept;
		BitStream(unsigned char* _data, const int lengthInBytes, bool _copyData) noexcept;
		virtual ~BitStream() noexcept;

		void Reset() noexcept
		{
			readOffset = 0;
			bitsUsed = 0;
		}

		size_t ReadOffset() noexcept
		{
			return readOffset;
		}

		inline void AlignWriteToByteBoundary() noexcept { bitsUsed += 8 - (((bitsUsed - 1) & 7) + 1); }

		inline void AlignReadToByteBoundary() noexcept { readOffset += 8 - (((readOffset - 1) & 7) + 1); }


		/*
		* Checks for free bits and allocates the needed number of bytes to fit the data
		*/
		bool AllocateBits(size_t numberOfBits) noexcept;


		void SetReadOffset(size_t readOffset) noexcept;

		/*
		* This functions checks for available bits and add bit if needed
		*/
		bool PrepareWrite(size_t bitsToWrite) noexcept;


		void SetWriteOffset(size_t writeOffset) noexcept;

		void AddWriteOffset(size_t writeOffset) noexcept;
		/*
		*
		*/
		void Write1() noexcept;
		void Write0() noexcept;

		inline bool ReadBit() noexcept
		{
			bool result = (pData[readOffset >> 3] & (0x80 >> (readOffset & 7))) != 0;
			readOffset++;
			return result;
		}

		bool WriteBits(const unsigned char *data, size_t numberOfBits) noexcept;
		bool Write(const unsigned char *data, size_t size) noexcept;
		bool Write(const char* data, size_t size) noexcept;

		char * Data() noexcept
		{
			return (char*)pData;
		}

		size_t Size() noexcept
		{
			return BitsToBytes(bitsUsed);
		}

		// At least 2 parameters
		template<typename T, typename T2, typename ...Ts>
		typename std::enable_if<std::is_pointer<T>::value == false, bool>::type
			Write(const T& value, const T2 &value2, const Ts&... values) noexcept
		{
			using ltype_const = typename std::remove_reference<T>::type;
			using ltype = typename std::remove_const<ltype_const>::type;

			if (!Write<ltype>(value))
				return false;

			return Write<T2, Ts...>(value2, values...);
		}

		/*template<typename T>
		typename std::enable_if<std::is_same<T, std::string>::value, bool>::type
		Write(const T& str) noexcept
		{
		return (Write(str.size())
		&& Write((unsigned char*)str.c_str(), str.size()));
		}*/

		template<typename T>
		bool Write(const T& value) noexcept
		{
			return Write(reinterpret_cast<const unsigned char*>(&value), sizeof(T));
		}

		bool ReadBits(char *pData, size_t  numberOfBits) noexcept;
		bool Read(char *pData, size_t size) noexcept;

		// At least 2 parameters
		template<typename T, typename T2, typename ...Ts>
		inline
			typename std::enable_if<std::is_pointer<T>::value == false, bool>::type // This makes 'bool Read(char *pData, size_t size) noexcept;' working
			Read(T&& value, T2&& value2, Ts&&... values) noexcept
		{
			using ltype = typename std::remove_reference<T>::type;

			if (!Read<ltype>(::std::forward<T>(value)))
				return false;

			return Read(::std::forward<T2>(value2), ::std::forward<Ts...>(values)...);
		}

		template<typename T>
		inline bool Read(T& value) noexcept
		{
			return Read((char*)&value, sizeof(T));
		}

		bool operator=(const BitStream &right) noexcept;
		bool operator=(BitStream &&right) noexcept;

	public: /* Stream operators */

		template<typename T>
		inline BitStream& operator<<(T &&value) noexcept
		{
			Write(::std::forward<T>(value));

			return *this;
		}

		template<typename T>
		inline BitStream& operator>>(T &&value) noexcept
		{
			Read(::std::forward<T>(value));

			return *this;
		}
	};

	template<>
	inline bool BitStream::Write(const std::string & str) noexcept
	{
		return (Write(str.size())
				&& Write((unsigned char*)str.c_str(), str.size()));
	}

	template<>
	inline bool BitStream::Write(const bool& value) noexcept
	{
		(value ? Write1() : Write0());
		return true;
	}

	template<>
	inline bool BitStream::Read(bool &value) noexcept
	{
		value = ReadBit();
		return true;
	}

	template<>
	inline bool BitStream::Read(std::string &value) noexcept
	{
		auto size = value.size();
		Read(size);

		value.resize(size);

		Read(const_cast<char*>(value.data()), size);

		return true;
	}

	inline BitStream::BitStream() noexcept
		: BitStream(10)
	{ }

	inline BitStream::BitStream(size_t initialBytes) noexcept
	{
		if (initialBytes < BITSTREAM_STACK_SIZE)
		{
			pData = stackData;
			bitsAllocated = BytesToBits(BITSTREAM_STACK_SIZE);
		}
		else
		{
			pData = new unsigned char[initialBytes];
			bitsAllocated = BytesToBits(initialBytes);
		}
	}

	inline BitStream::BitStream(BitStream &&other) noexcept
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

	inline BitStream::BitStream(unsigned char* _data, const int lengthInBytes, bool _copyData) noexcept
	{
		bitsUsed = BytesToBits(lengthInBytes);
		readOffset = 0;
		//copyData = _copyData;
		bitsAllocated = BytesToBits(lengthInBytes);

		if (_copyData || 1 == 1)
		{
			if (lengthInBytes < BITSTREAM_STACK_SIZE)
			{
				pData = stackData;
				bitsAllocated = BytesToBits(BITSTREAM_STACK_SIZE);
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

	inline BitStream::~BitStream() noexcept
	{
		if (pData != stackData)
			if (pData)
				free(pData);

		bitsAllocated = 0;
		bitsUsed = 0;
		readOffset = 0;
	}

	inline bool BitStream::AllocateBits(size_t numberOfBits) noexcept
	{
		size_t newBitsAllocated = bitsAllocated + numberOfBits;

		if (BitsToBytes(newBitsAllocated) < BITSTREAM_STACK_SIZE)
		{
			return true;
		}

		auto data = (decltype(pData))realloc(pData, BitsToBytes(newBitsAllocated));

		if (!data)
			return false;

		if (pData == stackData)
			memcpy(data, stackData, BitsToBytes(bitsAllocated));

		pData = data;

		assert(pData);

		bitsAllocated = newBitsAllocated;

		return true;
	}

	inline bool BitStream::PrepareWrite(size_t bitsToWrite) noexcept
	{
		if ((bitsAllocated - bitsUsed) >= bitsToWrite)
			return true;
		else
		{
			return AllocateBits(bitsToWrite);
		}
	}

	inline void BitStream::Write0() noexcept
	{
		PrepareWrite(1);

		// New bytes need to be zeroed
		if ((bitsUsed & 7) == 0)
			pData[bitsUsed >> 3] = 0;

		++bitsUsed;
	}

	inline void BitStream::Write1() noexcept
	{
		PrepareWrite(1);

		size_t numberOfBitsMod8 = bitsUsed & 7;

		if (numberOfBitsMod8 == 0)
			pData[bitsUsed >> 3] = 0x80;
		else
			pData[bitsUsed >> 3] |= 0x80 >> (numberOfBitsMod8); // Set the bit to 1

		++bitsUsed;
	}

	inline bool BitStream::Write(const unsigned char *data, size_t size) noexcept
	{
		PrepareWrite(BytesToBits(size));

		const size_t bitsToWrite = BytesToBits(size);

		if ((bitsUsed & 7) == 0)
		{
			memcpy(this->pData + BitsToBytes(bitsUsed), data, size);
			bitsUsed += bitsToWrite;
			return true;
		}
		else
		{
			return WriteBits(data, bitsToWrite);
		}
	}

	inline bool BitStream::Write(const char *pData, size_t size) noexcept
	{
		PrepareWrite(BytesToBits(size));

		// This might cause bad performance because we have todo the same in writebits. Question How can we optimize that?
		// Check if we can just use memcpy :)
		const size_t bitsToWrite = BytesToBits(size);

		if ((bitsUsed & 7) == 0)
		{
			memcpy(this->pData + BitsToBytes(bitsUsed), pData, size);
			bitsUsed += bitsToWrite;
			return true;
		}
		else
		{
			return WriteBits((unsigned char*)pData, bitsToWrite);
		}
	}


	inline bool BitStream::WriteBits(const unsigned char *pData, size_t  numberOfBits) noexcept
	{
		const size_t numberOfBitsUsedMod8 = bitsUsed & 7;

		if (numberOfBitsUsedMod8 == 0)
		{
			// Just do a memcpy instead of do this in the loop below
			memcpy(this->pData + BitsToBytes(bitsUsed), pData, BitsToBytes(numberOfBits));
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


	inline bool BitStream::Read(char *pData, size_t size) noexcept
	{
		if ((readOffset & 7) == 0)
		{
			memcpy(pData, this->pData + (readOffset >> 3), (size_t)size);

			readOffset += size << 3;
		}
		else
		{
			// Read dat bits
			ReadBits(pData, BytesToBits(size));
		}

		return true;
	}

	inline bool BitStream::ReadBits(char *pData, size_t  numberOfBits) noexcept
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

			memset(pData, 0, (size_t)BitsToBytes(numberOfBits));

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
					readOffset += numberOfBits;
					return true;
				}
			}

			return true;
		}
	}

	/*inline bool BitStream::Write(const std::string & str) noexcept
	{
	return (Write(str.size())
	&& Write((unsigned char*)str.c_str(), str.size()));
	}*/


	inline void BitStream::SetReadOffset(size_t readOffset) noexcept
	{
		this->readOffset = readOffset;
	}

	inline void BitStream::SetWriteOffset(size_t writeOffset) noexcept
	{
		PrepareWrite(writeOffset - this->bitsUsed);

		bitsUsed = writeOffset;
	}

	// Add write offset in bits
	inline void BitStream::AddWriteOffset(size_t writeOffset) noexcept
	{
		PrepareWrite(this->bitsUsed + writeOffset);

		bitsUsed += writeOffset;
	}

#pragma region operators

	inline bool BitStream::operator=(const BitStream &right) noexcept
	{
		PrepareWrite(right.bitsUsed);

		memcpy(this->pData, right.pData, BitsToBytes(right.bitsUsed));
		this->bitsUsed = right.bitsUsed;
		this->readOffset = right.readOffset;

		return true;
	}

	inline bool BitStream::operator=(BitStream &&right) noexcept
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
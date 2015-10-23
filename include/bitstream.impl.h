/*
	* Copyright (C) GTA:Multiplayer Team (https://wiki.gta-mp.net/index.php/Team)
	*
	* Redistribution and use in source and binary forms, with or without
	* modification, are permitted provided that the following conditions are
	* met:
	*
	*     * Redistributions of source code must retain the above copyright
	*		notice, this list of conditions and the following disclaimer.
	*     * Redistributions in binary form must reproduce the above
	*		copyright notice, this list of conditions and the following disclaimer
	*		in the documentation and/or other materials provided with the
	*		distribution.
	*     * Neither the name of GTA-Network nor the names of its
	*		contributors may be used to endorse or promote products derived from
	*		this software without specific prior written permission.
	*
	* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
	* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
	* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
	* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES INCLUDING, BUT NOT
	* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	* DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND ON ANY
	* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	* INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE
	* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#pragma once

#include "bitstream.h"
#include <cassert>

namespace knet
{
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
	{
	}

	inline BitStream::BitStream(size_t initialBytes) noexcept
	{
		if (initialBytes < BITSTREAM_STACK_SIZE)
		{
			pData = stackData;
			_bitsAllocated = BytesToBits(BITSTREAM_STACK_SIZE);
		}
		else
		{
			pData = new unsigned char[initialBytes];
			_bitsAllocated = BytesToBits(initialBytes);
		}
	}

	inline BitStream::BitStream(const BitStream &other) noexcept
		: BitStream()
	{
		PrepareWrite(other._bitsUsed);

		memcpy(this->pData, other.pData, BitsToBytes(other._bitsUsed));
		this->_bitsUsed = other._bitsUsed;
		this->_readOffset = other._readOffset;
		this->_maxWritten = other._maxWritten;
	}

	inline BitStream::BitStream(BitStream &&other) noexcept
	{
		assert(this != &other);

		if (pData != stackData)
			free(pData);

		if (other.pData == other.stackData)
			memcpy(this->stackData, other.stackData, BITSTREAM_STACK_SIZE);
		else
			pData = other.pData;

		_bitsAllocated = other._bitsAllocated;
		_bitsUsed = other._bitsUsed;
		_readOffset = other._readOffset;
		_maxWritten = other._maxWritten;

		other._bitsUsed = 0;
		other._bitsAllocated = 0;
		other.pData = nullptr;
		other._readOffset = 0;
		other._maxWritten = 0;
	}

	inline BitStream::BitStream(unsigned char* _data, size_t lengthInBytes, bool _copyData) noexcept
	{
		_bitsUsed = BytesToBits(lengthInBytes);
		_maxWritten = _bitsUsed;
		_readOffset = 0;
		//copyData = _copyData;
		_bitsAllocated = BytesToBits(lengthInBytes);

		if (_copyData || 1 == 1)
		{
			if (lengthInBytes < BITSTREAM_STACK_SIZE)
			{
				pData = stackData;
				_bitsAllocated = BytesToBits(BITSTREAM_STACK_SIZE);
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
			free(pData);

		pData = nullptr;
		_bitsAllocated = 0;
		_bitsUsed = 0;
		_readOffset = 0;
		_maxWritten = 0;
	}

	inline bool BitStream::AllocateBits(size_t numberOfBits) noexcept
	{
		size_t newBitsAllocated = _bitsUsed + numberOfBits;

		if (numberOfBits + _bitsUsed > 0 && ((_bitsAllocated - 1) >> 3) < ((newBitsAllocated - 1) >> 3))   // If we need to allocate 1 or more new bytes
		{
			newBitsAllocated = (numberOfBits + _bitsUsed) * 2;
			if (newBitsAllocated - (numberOfBits + _bitsUsed) > 1048576)
				newBitsAllocated = numberOfBits + _bitsUsed + 1048576;

			if (BitsToBytes(newBitsAllocated) > BITSTREAM_STACK_SIZE)
			{
				decltype(pData) data = pData;

				if (data == stackData)
				{
					// As stackData is a static array,
					// we have to use nullptr so realloc acts like malloc
					data = nullptr;
				}

				auto tdata = (decltype(pData))realloc(data, BitsToBytes(newBitsAllocated));

				if (!tdata)
				{
					free(data);
					data = nullptr;

					return false;
				}

				data = tdata;

				if (pData == stackData)
				{
					memcpy(data, stackData, BitsToBytes(_bitsAllocated));
				}

				pData = data;
				assert(pData);

				if (newBitsAllocated > _bitsAllocated)
					_bitsAllocated = newBitsAllocated;
			}
			else {
				return true;
			}
		}
		return true;
	}

	inline bool BitStream::PrepareWrite(size_t bitsToWrite) noexcept
	{
		if(_bitsUsed >= _maxWritten) {
			_maxWritten += bitsToWrite;
		}

		if ((_bitsAllocated - _bitsUsed) >= bitsToWrite)
			return true;
		else
		{
			// We basically need something like max written to!
			//
			return AllocateBits(bitsToWrite);
		}
	}

	inline void BitStream::Write0() noexcept
	{
		PrepareWrite(1);

		// New bytes need to be zeroed
		if ((_bitsUsed & 7) == 0)
			pData[_bitsUsed >> 3] = 0;

		++_bitsUsed;
	}

	inline bool BitStream::ReadBit() noexcept
	{
		bool result = (pData[_readOffset >> 3] & (0x80 >> (_readOffset & 7))) != 0;
		_readOffset++;
		return result;
	}

	inline void BitStream::Write1() noexcept
	{
		PrepareWrite(1);

		size_t numberOfBitsMod8 = _bitsUsed & 7;

		if (numberOfBitsMod8 == 0)
			pData[_bitsUsed >> 3] = 0x80;
		else
			pData[_bitsUsed >> 3] |= 0x80 >> (numberOfBitsMod8); // Set the bit to 1

		++_bitsUsed;
	}

	inline bool BitStream::Write(const unsigned char *data, size_t size) noexcept
	{
		PrepareWrite(BytesToBits(size));

		const size_t bitsToWrite = BytesToBits(size);

		if ((_bitsUsed & 7) == 0)
		{
			memcpy(this->pData + BitsToBytes(_bitsUsed), data, size);
			_bitsUsed += bitsToWrite;
			return true;
		}
		else
		{
			return WriteBits(data, bitsToWrite);
		}
	}

	inline bool BitStream::Write(const char *data, size_t size) noexcept
	{
		PrepareWrite(BytesToBits(size));

		// This might cause bad performance because we have todo the same in writebits. Question How can we optimize that?
		// Check if we can just use memcpy :)
		const size_t bitsToWrite = BytesToBits(size);

		if ((_bitsUsed & 7) == 0)
		{
			memcpy(this->pData + BitsToBytes(_bitsUsed), data, size);
			_bitsUsed += bitsToWrite;
			return true;
		}
		else
		{
			return WriteBits((unsigned char*)data, bitsToWrite);
		}
	}

	inline char * BitStream::Data() noexcept
	{
		return (char*)pData;
	}

	inline size_t BitStream::Size() noexcept
	{
		return BitsToBytes(_maxWritten);
	}


	inline bool BitStream::WriteBits(const unsigned char *data, size_t  numberOfBits) noexcept
	{
		const size_t numberOfBitsUsedMod8 = _bitsUsed & 7;

		if (numberOfBitsUsedMod8 == 0)
		{
			// Just do a memcpy instead of do this in the loop below
			memcpy(this->pData + BitsToBytes(_bitsUsed), data, BitsToBytes(numberOfBits));
			_bitsUsed += numberOfBits;

			return true;
		}

		unsigned char dataByte;
		const baseType * inputPtr = data;

		while (numberOfBits > 0)
		{
			dataByte = *(inputPtr++);

			// Copy over the new data.
			*(this->pData + (_bitsUsed >> 3)) |= dataByte >> (numberOfBitsUsedMod8);

			if (8 - (numberOfBitsUsedMod8) < 8 && 8 - (numberOfBitsUsedMod8) < numberOfBits)
			{
				*(this->pData + (_bitsUsed >> 3) + 1) = (unsigned char)(dataByte << (8 - (numberOfBitsUsedMod8)));
			}

			if (numberOfBits >= 8)
			{
				_bitsUsed += 8;
				numberOfBits -= 8;
			}
			else
			{
				_bitsUsed += numberOfBits;
				return true;
			}
		}

		return true;
	}


	inline bool BitStream::Read(char *data, size_t size) noexcept
	{
		if ((_readOffset & 7) == 0)
		{
			// Read is at normal byte boundary
			if(_maxWritten < (BytesToBits(size) + _readOffset))
				return false;

			memcpy(data, this->pData + (BitsToBytes(_readOffset)), (size_t)size);

			_readOffset += BytesToBits(size);
		}
		else
		{
			// Read dat bits
			return ReadBits(data, BytesToBits(size));
		}

		return true;
	}

	inline bool BitStream::ReadBits(char *data, size_t  numberOfBits) noexcept
	{
		if (numberOfBits <= 0)
			return false;

		if (_readOffset + numberOfBits > _maxWritten)
			return false;

		const size_t readOffsetMod8 = _readOffset & 7;

		if (readOffsetMod8 == 0 && (numberOfBits & 7) == 0)
		{
			memcpy(data, this->pData + (_readOffset >> 3), numberOfBits >> 3);
			_readOffset += numberOfBits;
			return true;
		}
		else
		{
			size_t offset = 0;

			memset(data, 0, (size_t)BitsToBytes(numberOfBits));

			while (numberOfBits > 0)
			{
				*(data + offset) |= *(this->pData + (_readOffset >> 3)) << (readOffsetMod8);

				if (readOffsetMod8 > 0 && numberOfBits > 8 - (readOffsetMod8))
					*(data + offset) |= *(this->pData + (_readOffset >> 3) + 1) >> (8 - (readOffsetMod8));

				if (numberOfBits >= 8)
				{
					numberOfBits -= 8;
					_readOffset += 8;
					offset++;
				}
				else
				{
					_readOffset += numberOfBits;
					return true;
				}
			}

			return true;
		}
	}

	inline void BitStream::SetReadOffset(size_t readOffset) noexcept
	{
		this->_readOffset = readOffset;
	}

	inline void BitStream::SetWriteOffset(size_t writeOffset) noexcept
	{
		if (writeOffset >= this->_bitsUsed) {
			PrepareWrite(writeOffset - this->_bitsUsed);
		}

		_bitsUsed = writeOffset;
	}

	// Add write offset in bits
	inline void BitStream::AddWriteOffset(size_t writeOffset) noexcept
	{
		if (writeOffset >= this->_bitsUsed) {
			PrepareWrite(writeOffset - this->_bitsUsed);
		}

		_bitsUsed += writeOffset;
	}

	// At least 2 parameters

	template <typename T, typename T2, typename ...Ts>
	inline typename std::enable_if<std::is_pointer<T>::value == false, bool>::type BitStream::Write(const T & value, const T2 & value2, const Ts & ...values) noexcept
	{
		using ltype_const = typename std::remove_reference<T>::type;
		using ltype = typename std::remove_const<ltype_const>::type;

		if (!Write<ltype>(value))
			return false;

		return Write<T2, Ts...>(value2, values...);
	}

	template <typename T>
	inline bool BitStream::Write(const T & value) noexcept
	{
		return Write(reinterpret_cast<const unsigned char*>(&value), sizeof(T));
	}

	// At least 2 parameters
	template <typename T, typename T2, typename ...Ts>
	inline typename std::enable_if<std::is_pointer<T>::value == false, bool>::type BitStream::Read(T && value, T2 && value2, Ts && ...values) noexcept
	{
		using ltype = typename std::remove_reference<T>::type;

		if (!Read<ltype>(::std::forward<T>(value)))
			return false;

		return Read(::std::forward<T2>(value2), ::std::forward<Ts...>(values)...);
	}

	template <typename T>
	inline bool BitStream::Read(T & value) noexcept
	{
		return Read((char*)&value, sizeof(T));
	}

#pragma region operators

	inline bool BitStream::operator=(const BitStream &right) noexcept
	{
		PrepareWrite(right._bitsUsed);

		memcpy(this->pData, right.pData, BitsToBytes(right._bitsUsed));
		this->_bitsUsed = right._bitsUsed;
		this->_readOffset = right._readOffset;

		return true;
	}

	inline bool BitStream::operator=(BitStream &&right) noexcept
	{
		assert(this != &right);

		if (pData != stackData)
			free(pData);

		if (right.pData == right.stackData)
			memcpy(this->stackData, right.stackData, BITSTREAM_STACK_SIZE);
		else
			pData = right.pData;

		_bitsAllocated = right._bitsAllocated;
		_bitsUsed = right._bitsUsed;
		_readOffset = right._readOffset;

		right._bitsUsed = 0;
		right._bitsAllocated = 0;
		right.pData = nullptr;
		right._readOffset = 0;

		return true;
	}



#pragma endregion
}

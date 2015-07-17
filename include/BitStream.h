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

#include <cstdint>
#include <type_traits>
#include <string>

#include <cassert>

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
		using baseType = uint8_t;


	private:
		baseType* _data = nullptr;
		size_t _readOffset = 0;
		size_t _bitsAllocated = 0;
		size_t _bitsUsed = 0;

		baseType stackData[BITSTREAM_STACK_SIZE];
	public:
		BitStream() noexcept;

		BitStream(size_t) noexcept;

		BitStream(BitStream &&) noexcept;
		BitStream(unsigned char*,const size_t, bool) noexcept;
		virtual ~BitStream() noexcept;

		void Reset() noexcept
		{
			_readOffset = 0;
			_bitsUsed = 0;
		}

		size_t ReadOffset() noexcept
		{
			return _readOffset;
		}

		inline void AlignWriteToByteBoundary() noexcept { _bitsUsed += 8 - (((_bitsUsed - 1) & 7) + 1); }

		inline void AlignReadToByteBoundary() noexcept { _readOffset += 8 - (((_readOffset - 1) & 7) + 1); }


		/*
		* Checks for free bits and allocates the needed number of bytes to fit the data
		*/
		bool AllocateBits(size_t) noexcept;


		void SetReadOffset(size_t) noexcept;

		/*
		* This functions checks for available bits and add bit if needed
		*/
		bool PrepareWrite(size_t) noexcept;


		void SetWriteOffset(size_t) noexcept;

		void AddWriteOffset(size_t) noexcept;
		/*
		*
		*/
		void Write1() noexcept;
		void Write0() noexcept;

		inline bool ReadBit() noexcept
		{
			bool result = (_data[_readOffset >> 3] & (0x80 >> (_readOffset & 7))) != 0;
			_readOffset++;
			return result;
		}

		bool WriteBits(const unsigned char *, size_t) noexcept;
		bool Write(const unsigned char *, size_t) noexcept;
		bool Write(const char*, size_t) noexcept;

		char * Data() noexcept
		{
			return (char*)_data;
		}

		size_t Size() noexcept
		{
			return BitsToBytes(_bitsUsed);
		}

		// At least 2 parameters
		template<typename T, typename T2, typename ...Ts>
		typename std::enable_if<std::is_pointer<T>::value == false, bool>::type
			Write(const T& value, const T2 &value2, const Ts&... values) noexcept
		{
			using ltype = std::decay_t<T>;

			if (!Write<ltype>(value))
				return false;

			return Write<T2, Ts...>(value2, values...);
		}

		template<typename T>
		bool Write(const T& value) noexcept
		{
			return Write(reinterpret_cast<const unsigned char*>(&value), sizeof(T));
		}

		bool ReadBits(char *, size_t) noexcept;
		bool Read(char *, size_t) noexcept;

		// At least 2 parameters
		template<typename T, typename T2, typename ...Ts>
		inline
			typename std::enable_if<std::is_pointer<T>::value == false, bool>::type // This makes 'bool Read(char *pData, size_t size) noexcept;' working
			Read(T&& value, T2&& value2, Ts&&... values) noexcept
		{
			using ltype = std::decay_t<T>;

			if (!Read<ltype>(::std::forward<T>(value)))
				return false;

			return Read(::std::forward<T2>(value2), ::std::forward<Ts...>(values)...);
		}

		template<typename T>
		inline bool Read(T& value) noexcept
		{
			return Read((char*)&value, sizeof(T));
		}

		bool operator=(const BitStream &) noexcept;
		bool operator=(BitStream &&) noexcept;

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
			_data = stackData;
			_bitsAllocated = BytesToBits(BITSTREAM_STACK_SIZE);
		}
		else
		{
			_data = new unsigned char[initialBytes];
			_bitsAllocated = BytesToBits(initialBytes);
		}
	}

	inline BitStream::BitStream(BitStream &&other) noexcept
	{
		if (_data != stackData)
			if (_data)
			free(_data);

		if (other._data == other.stackData)
			memcpy(this->stackData, other.stackData, BITSTREAM_STACK_SIZE);
		else
			_data = other._data;

		_bitsAllocated = other._bitsAllocated;
		_bitsUsed = other._bitsUsed;
		_readOffset = other._readOffset;

		other._bitsUsed = 0;
		other._bitsAllocated = 0;
		other._data = nullptr;
		other._readOffset = 0;
	}

	inline BitStream::BitStream(unsigned char* _data, const size_t lengthInBytes, bool _copyData) noexcept
	{
		_bitsUsed = BytesToBits(lengthInBytes);
		_readOffset = 0;
		//copyData = _copyData;
		_bitsAllocated = BytesToBits(lengthInBytes);

		if (_copyData || 1 == 1)
		{
			if (lengthInBytes < BITSTREAM_STACK_SIZE)
			{
				_data = stackData;
				_bitsAllocated = BytesToBits(BITSTREAM_STACK_SIZE);
			}
			else
				//if (lengthInBytes > 0)
			{
				_data = (unsigned char*)malloc((size_t)lengthInBytes);

				assert(_data);


			}
			/*else
			pData = 0;*/

			memcpy(_data, _data, (size_t)lengthInBytes);
		}
		else
			_data = (unsigned char*)_data;
	}

	inline BitStream::~BitStream() noexcept
	{
		if (_data != stackData)
			if (_data)
			free(_data);

		_bitsAllocated = 0;
		_bitsUsed = 0;
		_readOffset = 0;
	}

	inline bool BitStream::AllocateBits(size_t numberOfBits) noexcept
	{
		size_t newBitsAllocated = _bitsAllocated + numberOfBits;

		if (BitsToBytes(newBitsAllocated) < BITSTREAM_STACK_SIZE)
		{
			return true;
		}
		decltype(_data) data = _data;

		if (data == stackData)
		{
			// As stackData is a static array, 
			// we have to use nullptr so realloc acts like malloc
			data = nullptr;
		}

		data = (decltype(_data))realloc(data, BitsToBytes(newBitsAllocated));

		if (!data)
			return false;

		if (_data == stackData)
			memcpy(data, stackData, BitsToBytes(_bitsAllocated));

		_data = data;

		assert(_data);

		_bitsAllocated = newBitsAllocated;

		return true;
	}

	inline bool BitStream::PrepareWrite(size_t bitsToWrite) noexcept
	{
		if ((_bitsAllocated - _bitsUsed) >= bitsToWrite)
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
		if ((_bitsUsed & 7) == 0)
			_data[_bitsUsed >> 3] = 0;

		++_bitsUsed;
	}

	inline void BitStream::Write1() noexcept
	{
		PrepareWrite(1);

		size_t numberOfBitsMod8 = _bitsUsed & 7;

		if (numberOfBitsMod8 == 0)
			_data[_bitsUsed >> 3] = 0x80;
		else
			_data[_bitsUsed >> 3] |= 0x80 >> (numberOfBitsMod8); // Set the bit to 1

		++_bitsUsed;
	}

	inline bool BitStream::Write(const unsigned char *data, size_t size) noexcept
	{
		PrepareWrite(BytesToBits(size));

		const size_t bitsToWrite = BytesToBits(size);

		if ((_bitsUsed & 7) == 0)
		{
			memcpy(this->_data + BitsToBytes(_bitsUsed), data, size);
			_bitsUsed += bitsToWrite;
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

		const size_t bitsToWrite = BytesToBits(size);

		if ((_bitsUsed & 7) == 0)
		{
			memcpy(this->_data + BitsToBytes(_bitsUsed), pData, size);
			_bitsUsed += bitsToWrite;
			return true;
		}
		else
		{
			return WriteBits((unsigned char*)pData, bitsToWrite);
		}
	}

	inline bool BitStream::WriteBits(const unsigned char *pData, size_t  numberOfBits) noexcept
	{
		const size_t numberOfBitsUsedMod8 = _bitsUsed & 7;

		if (numberOfBitsUsedMod8 == 0)
		{
			// Just do a memcpy instead of do this in the loop below
			memcpy(this->_data + BitsToBytes(_bitsUsed), pData, BitsToBytes(numberOfBits));
			_bitsUsed += numberOfBits;

			return true;
		}

		unsigned char dataByte;
		const baseType * inputPtr = pData;

		while (numberOfBits > 0)
		{
			dataByte = *(inputPtr++);

			// Copy over the new data.
			*(this->_data + (numberOfBits >> 3)) |= dataByte >> (numberOfBitsUsedMod8);

			if (8 - (numberOfBitsUsedMod8) < 8 && 8 - (numberOfBitsUsedMod8) < numberOfBits)
			{
				*(this->_data + (_bitsUsed >> 3) + 1) = (unsigned char)(dataByte << (8 - (numberOfBitsUsedMod8)));
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


	inline bool BitStream::Read(char *pData, size_t size) noexcept
	{
		if ((_readOffset & 7) == 0)
		{
			memcpy(pData, this->_data + (_readOffset >> 3), (size_t)size);

			_readOffset += size << 3;
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

		if (_readOffset + numberOfBits > _bitsUsed)
			return false;


		const size_t readOffsetMod8 = _readOffset & 7;


		if (readOffsetMod8 == 0 && (numberOfBits & 7) == 0)
		{
			memcpy(pData, this->_data + (_readOffset >> 3), numberOfBits >> 3);
			_readOffset += numberOfBits;
			return true;
		}
		else
		{
			size_t offset = 0;

			memset(pData, 0, (size_t)BitsToBytes(numberOfBits));

			while (numberOfBits > 0)
			{
				*(pData + offset) |= *(this->_data + (_readOffset >> 3)) << (readOffsetMod8);

				if (readOffsetMod8 > 0 && numberOfBits > 8 - (readOffsetMod8))
					*(pData + offset) |= *(this->_data + (_readOffset >> 3) + 1) >> (8 - (readOffsetMod8));

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
		PrepareWrite(writeOffset - this->_bitsUsed);

		_bitsUsed = writeOffset;
	}

	// Add write offset in bits
	inline void BitStream::AddWriteOffset(size_t writeOffset) noexcept
	{
		PrepareWrite(this->_bitsUsed + writeOffset);

		_bitsUsed += writeOffset;
	}

#pragma region operators

	inline bool BitStream::operator=(const BitStream &right) noexcept
	{
		PrepareWrite(right._bitsUsed);

		memcpy(this->_data, right._data, BitsToBytes(right._bitsUsed));
		this->_bitsUsed = right._bitsUsed;
		this->_readOffset = right._readOffset;

		return true;
	}

	inline bool BitStream::operator=(BitStream &&right) noexcept
	{
		if (_data != stackData)
			if (_data)
			free(_data);

		if (right._data == right.stackData)
			memcpy(this->stackData, right.stackData, BITSTREAM_STACK_SIZE);
		else
			_data = right._data;

		_bitsAllocated = right._bitsAllocated;
		_bitsUsed = right._bitsUsed;
		_readOffset = right._readOffset;

		right._bitsUsed = 0;
		right._bitsAllocated = 0;
		right._data = nullptr;
		right._readOffset = 0;

		return true;
	}

#pragma endregion
}
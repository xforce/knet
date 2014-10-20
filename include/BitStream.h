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

template<typename T>
inline constexpr T BITS_TO_BYTES(T x)
{
	return (((x) +7) >> 3);
}

template<typename T>
inline constexpr T BYTES_TO_BITS(T x)
{
	return ((x) << 3);
}

#define BITSTREAM_STACK_SIZE 256

namespace knet
{
	class KNET_API BitStream
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
			return (char*) pData;
		}

		size_t Size() noexcept
		{
			return BITS_TO_BYTES(bitsUsed);
		}

		// At least 2 parameters
		template<typename T, typename T2, typename ...Ts>
		inline
		typename std::enable_if<std::is_pointer<T>::value == false, bool>::type
		Write(const T& value, const T2 &value2, const Ts&... values) noexcept
		{
			using ltype_const = typename std::remove_reference<T>::type;
			using ltype = typename std::remove_const<ltype_const>::type;

			if (!Write<ltype>(value))
				return false;

			return Write<T2, Ts...>(value2, values...);
		}

		template<typename T>
		inline bool Write(const T& value) noexcept
		{
			return Write(reinterpret_cast<const unsigned char*>(&value), sizeof(T));
		}

		template<>
		bool Write<std::string>(const std::string &str) noexcept;

		inline bool Write(const bool& value) noexcept
		{
			(value ? Write1() : Write0());
			return true;
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
			return Read((char*) &value, sizeof(T));
		}

		template<>
		inline bool Read<bool>(bool &value) noexcept
		{
			value = ReadBit();
			return true;
		}

		template<>
		inline bool Read<std::string>(std::string &value) noexcept
		{
			auto size = value.size();
			Read(size);

			value.resize(size);

			Read((char*) value.data(), size);

			return true;
		}

		bool operator=(const BitStream &right) noexcept;
		bool operator=(BitStream &&right) noexcept;

	public: /* Stream operators */

		template<typename T>
		inline BitStream& operator<<(const T &&value) noexcept
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

NAMESPACE_KNET_END
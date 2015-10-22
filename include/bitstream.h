
#pragma once

#include <cstdint>

#include <type_traits>

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

#define BITSTREAM_STACK_SIZE 128

	class BitStream
	{
	public:
		using baseType = unsigned char;


	private:
		baseType* pData = nullptr;
		size_t _readOffset = 0;
		size_t _bitsAllocated = 0;
		size_t _bitsUsed = 0;

		baseType stackData[BITSTREAM_STACK_SIZE];
	public:
		BitStream() noexcept;

		BitStream(size_t initialBytes) noexcept;

		BitStream(const BitStream &other) noexcept;
		BitStream(BitStream &&other) noexcept;
		BitStream(unsigned char* _data, size_t lengthInBytes, bool _copyData) noexcept;
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

		size_t WriteOffset() noexcept
		{
			return _bitsUsed;
		}

		inline void AlignWriteToByteBoundary() noexcept { _bitsUsed += 8 - (((_bitsUsed - 1) & 7) + 1); }

		inline void AlignReadToByteBoundary() noexcept { _readOffset += 8 - (((_readOffset - 1) & 7) + 1); }


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

		char * Data() noexcept;

		size_t Size() noexcept;

		/*
			Write stuff
		*/
		void Write1() noexcept;
		void Write0() noexcept;
		bool WriteBits(const unsigned char *data, size_t numberOfBits) noexcept;

		bool Write(const unsigned char *data, size_t size) noexcept;
		bool Write(const char* data, size_t size) noexcept;

		// At least 2 parameters
		template<typename T, typename T2, typename ...Ts>
		typename std::enable_if<std::is_pointer<T>::value == false, bool>::type
		Write(const T& value, const T2 &value2, const Ts&... values) noexcept;

		template<typename T>
		bool Write(const T& value) noexcept;

		/*
			Read stuff
		*/
		bool ReadBit() noexcept;
		bool ReadBits(char *pData, size_t  numberOfBits) noexcept;
		bool Read(char *pData, size_t size) noexcept;

		// At least 2 parameters
		template<typename T, typename T2, typename ...Ts>
		typename std::enable_if<std::is_pointer<T>::value == false, bool>::type // This makes 'bool Read(char *pData, size_t size) noexcept;' working
		Read(T&& value, T2&& value2, Ts&&... values) noexcept;

		template<typename T>
		bool Read(T& value) noexcept;

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
}

#include "bitstream.impl.h"

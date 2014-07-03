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

#define BITS_TO_BYTES(x) (((x)+7)>>3)
#define BYTES_TO_BITS(x) ((x)<<3)

#define BITSTREAM_STACK_SIZE 256

namespace keksnl
{
	class BitStream
	{
	public:
		typedef unsigned char baseType;


	private:
		baseType* pData = nullptr;
		size_t readOffset = 0;
		size_t bitsAllocated = 0;
		size_t bitsUsed = 0;

		baseType stackData[BITSTREAM_STACK_SIZE];

		
	public:
		BitStream();
		BitStream(size_t initialBytes);
		BitStream(BitStream &&other);
		BitStream(unsigned char* _data, const int lengthInBytes, bool _copyData);
		virtual ~BitStream();

		void Reset()
		{
			readOffset = 0;
			bitsUsed = 0;
		}

		size_t ReadOffset()
		{
			return readOffset;
		}

		/*
		* Checks for free bits and allocates the needed number of bytes to fit the data
		*/
		bool AllocateBits(size_t numberOfBits);

		
		void SetReadOffset(size_t readOffset);

		/*
		* This functions checks for available bits and add bit if needed
		*/
		bool PrepareWrite(size_t bitsToWrite);


		void SetWriteOffset(size_t writeOffset);

		void AddWriteOffset(size_t writeOffset);
		/*
		* 
		*/
		void Write1();
		void Write0();

		inline bool ReadBit()
		{
			bool result = (pData[readOffset >> 3] & (0x80 >> (readOffset & 7))) != 0;
			readOffset++;
			return result;
		}

		bool WriteBits(const unsigned char *data, size_t  numberOfBits);
		bool Write(const unsigned char *data, size_t size);
		bool Write(const char* data, size_t size);
		bool Write(const std::string &str);
		//bool Write(size_t value);
		//bool Write(int value);
		//bool Write(bool value);
		//bool Write(long value);
		//bool Write(unsigned long value);
		//bool Write(short value);
		//bool Write(unsigned short value);

		char * Data()
		{
			return (char*)pData;
		}

		size_t Size()
		{
			return BITS_TO_BYTES(bitsUsed);
		}

		template<typename T>
		inline bool Write(const T& value)
		{
			return Write((unsigned char*)&value, sizeof(T));
		}

		inline bool Write(const bool& value)
		{
			(value ? Write1() : Write0());
			return true;
		}

		

		bool ReadBits(char *pData, size_t  numberOfBits);
		bool Read(char *pData, size_t size);

		template<typename T>
		inline bool Read(T& value)
		{
			return Read((char*)&value, sizeof(T));
		}

		inline bool Read(bool &value)
		{
			value = ReadBit();
			return true;
		}

		bool operator=(const BitStream &right);
		bool operator=(BitStream &&right);

	public: /* Stream operators */

		BitStream& operator<<(const std::string &str)
		{
			Write(str);
			return *this;
		}

		template<typename T>
		inline BitStream& operator<<(const T &value)
		{
			Write((unsigned char*)&value, sizeof(T));

			return *this;
		}

		template<typename T>
		inline BitStream& operator>>(const T &value)
		{
			Read((char*)&value, sizeof(T));

			return *this;
		}
		
	};
};
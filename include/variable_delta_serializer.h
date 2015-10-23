// Copyright 2015 the kNet authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "bitstream.h"
#include <unordered_map>

// Basically the same as the stuff in RakNet
// Just a bit cleaner
// FIXME: more comments

namespace net
{
	class VariableDeltaListEntry
	{
	private:
		char* _memory = nullptr;
		size_t _size = 0;
	public:
		virtual ~VariableDeltaListEntry()
		{
			free(_memory);
			_memory = nullptr;
			_size = 0;
		};

		template<typename T>
		void Set(T& value)
		{
			_memory = realloc(_memory, sizeof(T));
			if (_memory)
				memcpy(_memory, &value, sizeof(T));
			_size = sizeof(T);
		}

		void Set(const char* data, size_t length)
		{
			// Realloc allocates memory if the pointer is nullptr
			_memory = (char*)realloc(_memory, length);
			if (_memory)
				memcpy(_memory, data, length);
			_size = length;
		}

		char* Data()
		{
			assert(_memory != nullptr);
			return _memory;
		}

		size_t Size() const
		{
			return _size;
		}
	};

	class VariableDeltaList
	{
	private:
		uint32_t _writeIndex = 0;
		std::unordered_map<uint32_t, VariableDeltaListEntry> _variableList;
	public:
		void Start()
		{
			_writeIndex = 0;
		}

		// Returns true if the value as different than before
		template <typename T>
		bool WriteVariable(T &value)
		{
			BitStream temp;
			temp.Write(value);
			if (_writeIndex > _variableList.size())
			{
				VariableDeltaListEntry entry;
				entry.Set(temp.Data(), temp.Size());
				_variableList[_writeIndex] = entry;
				++_writeIndex;
				return true;
				// The value is new so return true and write it in our list
			}
			else
			{
				if (_variableList[_writeIndex].Size() != temp.Size())
				{
					// The value is different, because the size is different
					// Update the data
					_variableList[_writeIndex].Set(temp.Data(), temp.Size());
					++_writeIndex;
					return true;
				}
				else if (memcmp(_variableList[_writeIndex].Data(), temp.Data(), temp.Size()) == 0)
				{
					// The value is the same
					++_writeIndex;
					return false;
				}
				else
				{
					// The value is different
					_variableList[_writeIndex].Set(temp.Data(), temp.Size());
					++_writeIndex;
                    return true;
				}
			}
		}
	};

	class VariableDeltaSerializer
	{
	private:
		std::unordered_map<uint64_t, VariableDeltaList> _variableLists;
		std::unordered_map<uint64_t, BitStream&> _serializeBitStreams;
		std::unordered_map<uint64_t, BitStream&> _deserializeBitStreams;

	public:

		// The identifier here is used to have more than one VariableList if you want to sync per entry
		void BeginSerialze(BitStream &bitStream, uint64_t identifier = 0)
		{
			_serializeBitStreams[identifier] = bitStream;
			_variableLists[identifier].Start();
		}
		// Should be a move instruction
		void EndSerialize(uint64_t identifier = 0)
		{
			_serializeBitStreams.erase(identifier);
		}

		void BeginDeserialze(BitStream &bitStream, uint64_t identifier = 0)
		{
			_deserializeBitStreams[identifier] = bitStream;
		}

		void EndDeserialize(uint64_t identifier = 0)
		{
			_deserializeBitStreams.erase(identifier);
		}

		template<typename T>
		bool SerializeVariable(T& value, uint64_t identifier = 0)
		{
			bool differ = _variableLists[identifier].WriteVariable(value);
			auto& bitStream = _serializeBitStreams[identifier];
			bitStream.Write(differ);
			if (differ)
			{
				bitStream.Write(value);
			}

			return differ;
		}

		template<typename T>
		bool WriteDelta(T& value, uint64_t identifier = 0)
		{
			SerializeVariable(value, identifier);
		}

		template<typename T>
		bool Write(T& value, uint64_t identifier = 0)
		{
			SerializeVariable(value, identifier);
		}

		template<typename T>
		bool DeserializeVariable(T &variable, BitStream& bitStream, uint64_t identifier = 0)
		{
			if (bitStream.ReadBit())
			{
				bitStream.Read(variable);
				return true;
			}
			return false;
		}
	};
};

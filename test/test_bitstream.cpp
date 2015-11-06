// Copyright 2015 the kNet authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <bitstream.h>
#include <variable_delta_serializer.h>


TEST(BitStreamTest, ReadWriteString)
{
	std::string readStr;

	knet::BitStream bitStream;

	bitStream.Write(std::string("testString"));
	bitStream.Read(readStr);

	EXPECT_EQ("testString", readStr);
}

TEST(BitStreamTest, ReadWriteMultipleStrings)
{
	std::string readStr;
	std::string readStr2;

	knet::BitStream bitStream;

	bitStream.Write(std::string("testString"), std::string("testString2"));
	bitStream.Read(readStr, readStr2);

	EXPECT_EQ("testString", readStr);
	EXPECT_EQ("testString2", readStr2);
}

TEST(BitStreamTest, WriteOffset)
{
	std::string readStr;

	knet::BitStream bitStream;

	uint16_t i = 10000;

	bitStream.AddWriteOffset(knet::BytesToBits(sizeof(i)));

	bitStream.Write(std::string("testString"));

	bitStream.SetWriteOffset(0);

	bitStream.Write(i);

	i = 0;

	bitStream.Read(i, readStr);

	EXPECT_EQ(10000, i);
	EXPECT_EQ("testString", readStr);
}

TEST(BitStreamTest, ReadOffset)
{
	std::string readStr;

	knet::BitStream bitStream;

	uint16_t i = 10000;

	bitStream.Write(i, std::string("testString"));

	bitStream.SetReadOffset(knet::BytesToBits(sizeof(i)));

	i = 0;

	bitStream.Read(readStr);

	bitStream.SetReadOffset(0);

	bitStream.Read(i);

	EXPECT_EQ("testString", readStr);
	EXPECT_EQ(10000, i);
}

TEST(BitStreamTest, StreamOperators)
{
	std::string testStr = "This is a test string";

	knet::BitStream bitStream;

	bitStream << testStr;
	bitStream >> testStr;

	EXPECT_EQ("This is a test string", testStr);
}

TEST(BitStreamTest, AlignToByteBoundary)
{
	std::string testStr = "This is a test string";

	knet::BitStream bitStream;


	bitStream.Write0();
	bitStream.AlignWriteToByteBoundary();
	bitStream.Write1();
	bitStream.AlignWriteToByteBoundary();


	EXPECT_EQ(false, bitStream.ReadBit());
	bitStream.AlignReadToByteBoundary();

	EXPECT_EQ(8, bitStream.ReadOffset());

	EXPECT_EQ(true, bitStream.ReadBit());
}

TEST(BitStreamTest, BigDataReadWrite)
{
	char bigDataWrite[1024 * 100] = { 1 };
	knet::BitStream bitStream;

	bitStream.Write(bigDataWrite);

	char bigDataRead[1024 * 100] = { 0 };
	bitStream.Read(bigDataRead);

	EXPECT_EQ(bigDataWrite[0], bigDataRead[0]);
}

TEST(VariableDeltaSerializeTest, BasicReadWriteTest)
{
	knet::VariableDeltaSerializer serializer;

	knet::BitStream bitStream;
	knet::BitStream bitStream2;
	serializer.BeginSerialze(bitStream);

	std::string testString = "This is a text";
	uint32_t rValue = 10000;

	serializer.Write<uint32_t>(rValue);
	serializer.Write(testString);

	serializer.EndSerialize();

	// Second writer
	serializer.BeginSerialze(bitStream2, 1);

	serializer.Write<uint32_t>(rValue, 1);
	serializer.Write(testString, 1);

	serializer.EndSerialize(1);



	serializer.BeginDeserialze(bitStream);

	serializer.Read(rValue);
	serializer.Read(testString);
	
	serializer.EndDeserialize();

	EXPECT_EQ("This is a text", testString);
	EXPECT_EQ(10000, rValue);


	serializer.BeginDeserialze(bitStream2, 1);

	serializer.Read(rValue, 1);
	serializer.Read(testString, 1);

	serializer.EndDeserialize(1);

	EXPECT_EQ("This is a text", testString);
	EXPECT_EQ(10000, rValue);
}

TEST(VariableDeltaSerializeTest, AdvancedReadWriteTest)
{
	knet::VariableDeltaSerializer serializer;

	knet::BitStream bitStream;

	float fValue1 = 1;
	uint32_t rValue = 1;

	// changing values
	for (int i = 0; i < 10; ++i)
	{
		serializer.BeginSerialze(bitStream, 1);

		serializer.Write<uint32_t>(i, 1);
		serializer.Write<float>(static_cast<float>(i), 1);

		serializer.EndSerialize(1);

		rValue = 0;
		fValue1 = 0;

		serializer.BeginDeserialze(bitStream, 1);

		serializer.Read(rValue, 1);
		serializer.Read(fValue1, 1);

		serializer.EndDeserialize(1);

		EXPECT_EQ(i, fValue1);
		EXPECT_EQ(i, rValue);

		bitStream.Reset();
	}

	// Same values all over again
	for (int i = 0; i < 10; ++i)
	{
		serializer.BeginSerialze(bitStream, 1);

		serializer.Write<uint32_t>(10000, 1);
		serializer.Write<float>(static_cast<float>(10000), 1);

		serializer.EndSerialize(1);

		rValue = 0;
		fValue1 = 0;

		serializer.BeginDeserialze(bitStream, 1);

		serializer.Read(rValue, 1);
		serializer.Read(fValue1, 1);

		serializer.EndDeserialize(1);

		EXPECT_EQ(10000, fValue1);
		EXPECT_EQ(10000, rValue);

		bitStream.Reset();
	}
}
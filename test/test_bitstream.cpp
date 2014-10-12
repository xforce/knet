

#include <gtest/gtest.h>

#include <BitStream.h>


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

	knet::uint16 i = 10000;

	bitStream.AddWriteOffset(BYTES_TO_BITS(sizeof(i)));

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

	knet::uint16 i = 10000;

	bitStream.Write(i, std::string("testString"));

	bitStream.SetReadOffset(BYTES_TO_BITS(sizeof(i)));

	i = 0;

	bitStream.Read(readStr);

	EXPECT_EQ("testString", readStr);
}
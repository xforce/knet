

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
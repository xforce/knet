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

#include <ReliabilityLayer.h>
#include <unordered_set>

namespace keksnl
{

	CReliabilityLayer::CReliabilityLayer(ISocket * pSocket)
		: m_pSocket(pSocket)
	{
		firstUnsentAck = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(std::chrono::milliseconds(0));
	}

	CReliabilityLayer::~CReliabilityLayer()
	{

	}

	/*
		TODO: change this to eventHandler for Socket
	*/
	bool CReliabilityLayer::OnReceive(InternalRecvPacket *packet)
	{
		if (eventHandler)
		{
			eventHandler.Call(ReliabilityEvents::RECEIVE, packet);
		}

		std::lock_guard<std::mutex> m{bufferMutex};

		// Add packet to buffer
		bufferedPacketQueue.push(packet);

		return true;
	}

	InternalRecvPacket* CReliabilityLayer::PopBufferedPacket()
	{
		std::lock_guard<std::mutex> m{bufferMutex};

		// Return nullptr if no packet in queue
		if (bufferedPacketQueue.empty())
			return nullptr;

		// Get next packet to process
		InternalRecvPacket *packet = bufferedPacketQueue.front();
		bufferedPacketQueue.pop();
		return packet;
	}

	void CReliabilityLayer::SetTimeout(const std::chrono::milliseconds &time)
	{
		m_msTimeout = time;
	}

	const std::chrono::milliseconds& CReliabilityLayer::GetTimeout()
	{
		return m_msTimeout;
	}

	void CReliabilityLayer::Send(char *data, size_t numberOfBitsToSend, PacketPriority priority, PacketReliability reliability)
	{
		if (priority == PacketPriority::IMMEDIATE)
		{
			// Just send the packet
			CBitStream bitStream{BITS_TO_BYTES(numberOfBitsToSend) + 20};


			DatagramPacket* pDatagramPacket = new DatagramPacket;

			pDatagramPacket->header.isACK = false;
			pDatagramPacket->header.isNACK = false;
			pDatagramPacket->header.sequenceNumber = flowControlHelper.GetSequenceNumber();

			

			pDatagramPacket->header.Serialize(bitStream);

			bitStream.Write(reliability);
			bitStream.Write<unsigned short>(BITS_TO_BYTES(numberOfBitsToSend));
			bitStream.Write(data, BITS_TO_BYTES(numberOfBitsToSend));

			resendBuffer.push_back({std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()), pDatagramPacket});

			if (m_pSocket)
				m_pSocket->Send(m_RemoteSocketAddress, bitStream.Data(), bitStream.Size());

			pDatagramPacket->bitStream = std::move(bitStream);

			return;
		}

		// TODO: queue packets and send them at next process
#if 1
		BufferedSendPacket sendPacket(data, BITS_TO_BYTES(numberOfBitsToSend));
		sendPacket.reliability = reliability;

		sendBuffer.push_back(std::move(sendPacket));

		return;
#endif

		// Now move the data to resendList
		// TODO
	}

	void CReliabilityLayer::Process()
	{
		auto curTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());

		if (firstUnsentAck.time_since_epoch().count() > 0 && ((curTime - firstUnsentAck).count() >= 5000 || acknowledgements.size() > 100))
			SendACKs();

		// TODO: process all network and buffered stuff
		InternalRecvPacket * pPacket = nullptr;
		while ((pPacket = PopBufferedPacket()) && pPacket != nullptr)
		{
			// TODO: handle return
			ProcessPacket(pPacket);

			// delete the packet after processing
			delete pPacket;
			pPacket = nullptr;
		}

#if 1
		CBitStream bitStream{MAX_MTU_SIZE};

		/*
		DatagramHeader dh;
		dh.isACK = false;
		dh.isNACK = false;
		dh.sequenceNumber = flowControlHelper.GetSequenceNumber();

		dh.Serialize(bitStream);
		*/

#if 1
		/* I think it better to do it beforce sending the new packets because of stuff */
		for (auto & resendPacket : resendBuffer)
		{
			if ((curTime - resendPacket.first).count() > 10000)
			{
				// Resend the packet

				if (m_pSocket)
					m_pSocket->Send(m_RemoteSocketAddress, resendPacket.second->bitStream.Data(), resendPacket.second->bitStream.Size());

				resendPacket.first = curTime;

				printf("Resent packet %d\n", resendPacket.second->header.sequenceNumber);

				// Dont remove them because we dont have received an ack and the sequence number is same
			}
		}
#endif


		sendBuffer.shrink_to_fit();
		
		if (sendBuffer.size())
		{
			DatagramPacket* pDatagramPacket = new DatagramPacket;

			pDatagramPacket->header.isACK = false;
			pDatagramPacket->header.isNACK = false;
			pDatagramPacket->header.sequenceNumber = flowControlHelper.GetSequenceNumber();

			// Send queued packets
			for (auto &packet : sendBuffer)
			{
				/* TODO move to datagram packet */


				/* TODO: Message number */
				/* TODO: create a message packet and add it to the DatagramPacket */
				bitStream.Write(packet.reliability);
				bitStream.Write<unsigned short>(BITS_TO_BYTES(packet.bitLength));
				bitStream.Write(packet.data, BITS_TO_BYTES(packet.bitLength));

				if (bitStream.Size() >= MAX_MTU_SIZE)
				{
					if (m_pSocket)
						m_pSocket->Send(m_RemoteSocketAddress, pDatagramPacket->bitStream.Data(), pDatagramPacket->bitStream.Size());

					pDatagramPacket->bitStream = std::move(bitStream);

					// Add the packet to the resend buffer
					resendBuffer.push_back({curTime, pDatagramPacket});


					// Create a new datagram packet
					pDatagramPacket = new DatagramPacket();

					bitStream.Reset();

					pDatagramPacket->header.isACK = false;
					pDatagramPacket->header.isNACK = false;
					pDatagramPacket->header.sequenceNumber = flowControlHelper.GetSequenceNumber();

					pDatagramPacket->Serialize(bitStream);
				}
			}

			if (sendBuffer.size())
				;// printf("Sending %d bytes\n", bitStream.Size());

			if (sendBuffer.size() && bitStream.Size() && m_pSocket)
			{
				m_pSocket->Send(m_RemoteSocketAddress, bitStream.Data(), bitStream.Size());

				pDatagramPacket->bitStream = std::move(bitStream);
				resendBuffer.push_back({curTime, pDatagramPacket});
			}

			if (resendBuffer.capacity() > 512)
				resendBuffer.shrink_to_fit();

			sendBuffer.clear();
		}
#endif

	}

	bool CReliabilityLayer::ProcessPacket(InternalRecvPacket *pPacket)
	{
		for (auto &remoteSystem : remoteList)
		{
			if (remoteSystem.address == pPacket->remoteAddress)
			{
				CBitStream bitStream{(unsigned char*)pPacket->data, pPacket->bytesRead, true};

				//printf("Process %d bytes\n", pPacket->bytesRead);

				// Handle Packet in Reliability Layer
				DatagramHeader dh;
				dh.Deserialize(bitStream);
	
				if (dh.isACK)
				{
					// Handle ACK Packet
					std::vector<std::pair<int, int>> ranges;

					int count = 0;
					bitStream.Read(count);

					int max;
					int min;

					for (int i = 0; i < count; ++i)
					{
						bitStream.Read(min);
						bitStream.Read(max);

						//printf("Got ACK for %d %d\n", min, max);

						ranges.push_back({min, max});
					}

					auto size = resendBuffer.size();

					for (int i = 0; i < size; ++i)
					{
						auto sequenceNumber = resendBuffer[i].second->header.sequenceNumber;
						auto isInAckRange = [](decltype(ranges)& vecRange, int sequenceNumber) -> bool
						{
							for (auto &k : vecRange)
							{
								if (sequenceNumber >= k.first && sequenceNumber <= k.second)
									return true;
							}
							return false;
						};

						if (isInAckRange(ranges, sequenceNumber))
						{
							
							delete resendBuffer[i].second;
							resendBuffer[i].second = nullptr;
							resendBuffer.erase(resendBuffer.begin() + i);
							//printf("Remove %d from resend list\n", i);
							--i;
							--size;
						}
					}

					resendBuffer.shrink_to_fit();
				}
				else if (dh.isNACK)
				{
					// Handle NACK Packet
				}
				else
				{
					for (auto i : acknowledgements)
					{

						if (dh.sequenceNumber == i)
							printf("Already in ACK list %d\n", i);
					}

					//printf("Got packet %d\n", dh.sequenceNumber);

					acknowledgements.push_back(dh.sequenceNumber);

					ReliablePacket packet;
					while (bitStream.ReadOffset() < BYTES_TO_BITS(bitStream.Size()))
					{
						packet.Deserialize(bitStream);

						// Handle DATA Packet
						if (packet.reliability >= PacketReliability::RELIABLE)
						{
							// ACK/NACK stuff for packet

							// TODO: basic check if packet is valid


							if (firstUnsentAck.time_since_epoch().count() == 0 || firstUnsentAck == firstUnsentAck.min())
								firstUnsentAck = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());

							if (eventHandler)
							{
								eventHandler.Call(ReliabilityEvents::HANDLE_PACKET, pPacket);

							}
						}
					}

					// Now process the packet
				}

				return true;
			}
		}


		// New connection
		// Handle new connection event
		if (eventHandler)
		{
			// Handle new connection 
			auto r = eventHandler.Call(ReliabilityEvents::NEW_CONNECTION, pPacket);
			if (r != eventHandler.NO_EVENT /* this should not happen */ && r != eventHandler.ALL_TRUE /* one handler refused the connection */)
			{
				// NOTIFY REMOTE SYSTEM THAT THE CONNECTION WAS REFUSED
				// Now call connection refused event

				// Return false because the packet was not handled
				return false;
			}
		}

		RemoteSystem system;
		// This is not correct 
		/* We have to create a new remote socket 
			But it its ok for testing purposes
		*/

		system.pSocket = pPacket->pSocket;
		system.address = pPacket->remoteAddress;
		remoteList.push_back(std::move(system));

		// Now handle the packet


		if (eventHandler)
		{
			eventHandler.Call(ReliabilityEvents::HANDLE_PACKET, pPacket);
		}

		return true;
	}

	ISocket * CReliabilityLayer::GetSocket()
	{
		return m_pSocket;
	}

	void CReliabilityLayer::SetSocket(ISocket * pSocket)
	{
		if (m_pSocket == pSocket)
			return;

		// TODO: reset reliability layer
		m_pSocket = pSocket;
	}

	const SocketAddress & CReliabilityLayer::GetRemoteAddress()
	{
		return m_RemoteSocketAddress;
	}

	void CReliabilityLayer::SetRemoteAddress(SocketAddress &socketAddress)
	{
		m_RemoteSocketAddress = socketAddress;
	}




	void CReliabilityLayer::SendACKs()
	{
		if (acknowledgements.size() == 0)
			return;

		bool rerun = false;

		acknowledgements.shrink_to_fit();

		// Sort the unsorted vector so we can write the ranges
		std::sort(acknowledgements.begin(), acknowledgements.end());

		CBitStream bitStream{MAX_MTU_SIZE};

		int min = -1;
		int max = 0;
		int writtenTo = 0;
		int writeCount = 0;

		// Now write the range stuff to the bitstream
		for (int i = 0; i < acknowledgements.size(); ++i)
		{
			if (acknowledgements[i] == (acknowledgements[i + 1] - 1))
			{ /* (Next-1) equals current, so its a range */

				if (min == -1)
					min = acknowledgements[i];

				max = acknowledgements[i];
			}
			else if (min == -1)
			{ /* Not a range just a single value,
			  because previous was not in row */

				min = acknowledgements[i];
				max = acknowledgements[i];

				bitStream.Write<SequenceNumberType>(min);
				bitStream.Write<SequenceNumberType>(max);

				// Track the index we have written to
				writtenTo = i;
				writeCount++;

				min = -1;
			}
			else
			{
				// First diff at next so write max to current and write info to bitStream
				max = acknowledgements[i];
				bitStream.Write<SequenceNumberType>(min);
				bitStream.Write<SequenceNumberType>(max);

				// Track the index we have written to
				writtenTo = i;
				writeCount++;

				min = -1;
			}

			if (bitStream.Size() >= 1000)
			{
				rerun = true;
				break;
			}
		}

		acknowledgements.erase(acknowledgements.begin(), acknowledgements.begin() + writtenTo + 1);

		keksnl::CBitStream out{bitStream.Size() + 20 /* guessed header size */};

		DatagramHeader dh;
		dh.isACK = true;
		dh.isNACK = false;

		dh.Serialize(out);

		out.Write(writeCount);
		out.Write(bitStream.Data(), bitStream.Size());

		if (m_pSocket)
			m_pSocket->Send(m_RemoteSocketAddress, out.Data(), out.Size());

		firstUnsentAck = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(std::chrono::milliseconds(0));;

		if (rerun)
			SendACKs();
	}
};
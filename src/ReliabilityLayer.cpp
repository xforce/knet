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
		firstUnsentAck = firstUnsentAck.min();
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

	void CReliabilityLayer::Send(char *data, size_t numberOfBitsToSend, PacketReliability reliability)
	{
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
		if (std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::system_clock::now() - firstUnsentAck).count() >= 5000)
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

		DatagramHeader dh;
		dh.isACK = false;
		dh.isNACK = false;
		dh.sequenceNumber = flowControlHelper.GetSequenceNumber();

		dh.Serialize(bitStream);

		sendBuffer.shrink_to_fit();

		// Send queued packets
		for (auto &packet : sendBuffer)
		{
			/* TODO: Message number */
			bitStream.Write(packet.reliability);
			bitStream.Write<unsigned short>(BITS_TO_BYTES(packet.bitLength));
			bitStream.Write(packet.data, BITS_TO_BYTES(packet.bitLength));

			if (bitStream.Size() >= MAX_MTU_SIZE)
			{
				if (m_pSocket)
					m_pSocket->Send(m_RemoteSocketAddress, bitStream.Data(), bitStream.Size());

				bitStream.Reset();

				dh.isACK = false;
				dh.isNACK = false;
				dh.sequenceNumber = flowControlHelper.GetSequenceNumber();

				dh.Serialize(bitStream);
			}
		}

		if (sendBuffer.size())
			;// printf("Sending %d bytes\n", bitStream.Size());

		if (sendBuffer.size() && bitStream.Size() && m_pSocket)
			m_pSocket->Send(m_RemoteSocketAddress, bitStream.Data(), bitStream.Size());

		sendBuffer.clear();
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
					printf("Got ACKs\n");

				}
				else if (dh.isNACK)
				{
					// Handle NACK Packet
				}
				else
				{
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
		remoteList.push_back(system);

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

		CBitStream bitStream;

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

		acknowledgements.erase(acknowledgements.begin(), acknowledgements.begin() + writtenTo+1);
		
		keksnl::CBitStream out;

		DatagramHeader dh;
		dh.isACK = true;
		dh.isNACK = false;

		dh.Serialize(out);

		out.Write(writeCount);
		out.Write(bitStream.Data(), bitStream.Size());

		if (m_pSocket)
			m_pSocket->Send(m_RemoteSocketAddress, out.Data(), out.Size());

		firstUnsentAck = firstUnsentAck.min();

		if (rerun)
			SendACKs();
	}
};
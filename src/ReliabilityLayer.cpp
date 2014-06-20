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

namespace keksnl
{
	static const std::chrono::milliseconds resendTime = std::chrono::milliseconds(10000);

	CReliabilityLayer::CReliabilityLayer(ISocket * pSocket)
		: m_pSocket(pSocket)
	{


		firstUnsentAck = firstUnsentAck.min(); //std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(std::chrono::milliseconds(0));
		lastReceiveFromRemote = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(std::chrono::milliseconds(0));

		for(int i = 0; i < orderingIndex.size(); ++i)
		{
			orderingIndex[i] = 0;
		}

		for (int i = 0; i < lastOrderedIndex.size(); ++i)
		{
			lastOrderedIndex[i] = 0;
		}

		resendBuffer.reserve(512);
	}

	CReliabilityLayer::~CReliabilityLayer()
	{

	}

	bool CReliabilityLayer::OnReceive(InternalRecvPacket *packet)
	{
		if (eventHandler)
		{
			eventHandler.Call(ReliabilityEvents::RECEIVE, packet);
		}

		std::lock_guard<std::mutex> m{bufferMutex};

		lastReceiveFromRemote = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());

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
			CBitStream bitStream{BITS_TO_BYTES(numberOfBitsToSend) + 20};

			// Just send the packet
			DatagramPacket* pDatagramPacket = new DatagramPacket;

			pDatagramPacket->header.isACK = false;
			pDatagramPacket->header.isNACK = false;

			if (reliability == PacketReliability::RELIABLE
				|| reliability == PacketReliability::RELIABLE_ORDERED)
			{
				pDatagramPacket->header.isReliable = true;
				pDatagramPacket->header.sequenceNumber = flowControlHelper.GetSequenceNumber();
			}
			else
			{
				pDatagramPacket->header.isReliable = false;
			}

			pDatagramPacket->GetSizeToSend();
			ReliablePacket sendPacket{data, BITS_TO_BYTES(numberOfBitsToSend)};
			sendPacket.reliability = reliability;
			sendPacket.priority = priority;


			if(reliability == PacketReliability::RELIABLE_ORDERED)
			{
				sendPacket.orderedInfo.index = orderingIndex[orderingChannel]++;
				sendPacket.orderedInfo.channel = orderingChannel;
			}

			pDatagramPacket->packets.push_back(std::move(sendPacket));

			pDatagramPacket->Serialize(bitStream);

			if (pDatagramPacket->header.isReliable)
			{
				resendBuffer.push_back({std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()), std::unique_ptr<DatagramPacket>(pDatagramPacket)});
			}
			if (m_pSocket)
				m_pSocket->Send(m_RemoteSocketAddress, bitStream.Data(), bitStream.Size());

			return;
		}
		else
		{
			ReliablePacket sendPacket{data, BITS_TO_BYTES(numberOfBitsToSend)};
			sendPacket.reliability = reliability;
			sendPacket.priority = priority;

			if(reliability == PacketReliability::RELIABLE_ORDERED)
			{
				sendPacket.orderedInfo.index = orderingIndex[orderingChannel]++;
				sendPacket.orderedInfo.channel = orderingChannel;
			}

			sendBuffer[sendPacket.priority].push_back(std::move(sendPacket));

			return;
		}
	}

	void CReliabilityLayer::Process()
	{
		auto curTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());

		if ((firstUnsentAck.min() == firstUnsentAck || firstUnsentAck.time_since_epoch().count() > 0) && ((curTime - firstUnsentAck).count() >= 500))
			SendACKs();


		if(m_pSocket)
		{
			// Check for timeout
			if(resendBuffer.size() && lastReceiveFromRemote.time_since_epoch().count() && ((curTime - lastReceiveFromRemote).count() >= 5000))
			{
				DEBUG_LOG("Disconnect: Timeout");
				// NOW send disconnect event so it will be removed in our peer
				// The Peer which handles this event should stop listening for this socket by calling StopReceiving
				// Then the peer should remove the Remote stuff on next process not in this handler
				eventHandler.Call(ReliabilityEvents::CONNECTION_LOST_TIMEOUT, m_RemoteSocketAddress, DisconnectReason::TIMEOUT);

			}
		}


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

#pragma region Ordered Packet stuff

		ProcessOrderedPackets(curTime);

#pragma endregion



#pragma region Resend Stuff

		ProcessResend(curTime);

#pragma endregion




#pragma region Send Stuff

		ProcessSend(curTime);

#pragma endregion
	}

	void CReliabilityLayer::ProcessResend(std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> &curTime)
	{
		CBitStream bitStream{MAX_MTU_SIZE};

		auto curResendTime = curTime - resendTime;

		/* I think it better to do it beforce sending the new packets because of stuff */
		for (auto &resendPacket : resendBuffer)
		{
			if(curResendTime > resendPacket.first)
			{
				bitStream.Reset();


				// Is it better to add the packet to the send buffer? could be useful for later congestion control
				
				// Resend the packet
				resendPacket.second->Serialize(bitStream);
				
				if (m_pSocket)
					m_pSocket->Send(m_RemoteSocketAddress, bitStream.Data(), bitStream.Size());

				resendPacket.first = curTime;

				DEBUG_LOG("Resent packet %d on {%p}", resendPacket.second->header.sequenceNumber, this);

				// Dont remove them because we dont have received an ack and the sequence number is same
			}
		}
	}

	void CReliabilityLayer::ProcessOrderedPackets(std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> &curTime)
	{
		int i = 0;
		uint16 lastIndex = 0;
		uint8 channel = 0;

		for (auto & orderedPackets : orderedPacketBuffer)
		{
			if (orderedPackets.size())
			{

				//DEBUG_LOG("Sort");

				std::stable_sort(orderedPackets.begin(), orderedPackets.end(), [](const ReliablePacket& packet, const ReliablePacket& packet_) -> bool
				{
					return (packet.sequenceNumber < packet_.sequenceNumber);
				});

				if (orderedPackets.size())
				{
					lastIndex = lastOrderedIndex[orderedPackets.begin()->orderedInfo.channel];

					for (auto &packet : orderedPackets)
					{

						if (firstUnsentAck.time_since_epoch().count() == 0 || firstUnsentAck == firstUnsentAck.min())
							firstUnsentAck = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());

						if (lastIndex > packet.orderedInfo.index)
						{
							lastIndex = packet.orderedInfo.index;

							//DEBUG_LOG("Process ordered %d on %p", packet.orderedInfo.index, this);
							if (packet.orderedInfo.index == 0)
								DEBUG_LOG("Process ordered 0 on %p", this);

							if (eventHandler)
							{
								eventHandler.Call<ReliablePacket &, SocketAddress&>(ReliabilityEvents::HANDLE_PACKET, packet, packet.socketAddress);
							}

							orderedPackets.erase(orderedPackets.begin());

							break;

						}
						else if (packet.orderedInfo.index > 0 && packet.orderedInfo.index != (lastIndex + 1))
						{
							break;
						}
						else
						{

							//DEBUG_LOG("Process ordered %d on %p", packet.orderedInfo.index, this);

							if (packet.orderedInfo.index == 0)
								DEBUG_LOG("Process ordered 0 on %p", this);

							lastIndex = packet.orderedInfo.index;
							if (eventHandler)
							{
								eventHandler.Call<ReliablePacket &, SocketAddress&>(ReliabilityEvents::HANDLE_PACKET, packet, packet.socketAddress);
							}
						}

						++i;
						channel = packet.orderedInfo.channel;
					}
				}

				lastOrderedIndex[channel] = lastIndex;

				if (i > 0)
					orderedPackets.erase(orderedPackets.begin(), orderedPackets.begin() + i);

				i = 0;
				lastIndex = 0;
			}
		}
	}

	void CReliabilityLayer::ProcessSend(std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> &curTime)
	{
		CBitStream bitStream{MAX_MTU_SIZE};

		// This fixes high memory consumption
		//if(sendBuffer.capacity() > 2048)
		//	sendBuffer.shrink_to_fit();

		// Send 2 high then 1 medium after 2 medium send 1 low

		PacketPriority nextPriority = PacketPriority::HIGH;

		ReliablePacket packet;

		{
			// Is it easy to use smart pointers in a good way here?

			/* TODO: kinda hacky, make it better :D */

			DatagramPacket* pReliableDatagramPacket = new DatagramPacket();
			DatagramPacket* pUnrealiableDatagramPacket = new DatagramPacket();

			/* Setup Unrealiable datagram packet */
			pUnrealiableDatagramPacket->header.isACK = false;
			pUnrealiableDatagramPacket->header.isNACK = false;
			pUnrealiableDatagramPacket->header.isReliable = false;

			/* Setup Reliable datagram packet */
			pReliableDatagramPacket->header.isACK = false;
			pReliableDatagramPacket->header.isNACK = false;
			pReliableDatagramPacket->header.isReliable = true;
			pReliableDatagramPacket->header.sequenceNumber = flowControlHelper.GetSequenceNumber();

			DatagramPacket * pCurrentPacket = pReliableDatagramPacket;

			int mediumsSent = 0;
			int lowSent = 0;
			int highSent = 0;



			auto GetNextPriority = [&]() -> PacketPriority {
				if (highSent == 2)
				{
					highSent = 0;
					if (mediumsSent == 2)
					{
						mediumsSent = 0;
						return PacketPriority::LOW;
					}
					else
					{
						mediumsSent++;
						return PacketPriority::MEDIUM;
					}
				}
				else
				{
					highSent++;
					return PacketPriority::HIGH;
				}
			};


			int bufIndex[PacketPriority::IMMEDIATE] = {0};

			for (int i = 0; i < 100; ++i)
			{
				nextPriority = GetNextPriority();

				for (PacketPriority prio = nextPriority; prio > PacketPriority::LOW; prio = (PacketPriority)(prio - 1))
				{
					nextPriority = prio;

					if (sendBuffer[prio].size() > bufIndex[prio])						
						break;	
				}

				if (sendBuffer[nextPriority].size() == 0 || sendBuffer[nextPriority].size() <= bufIndex[nextPriority])
					break;


				packet = std::move(sendBuffer[nextPriority].at(bufIndex[nextPriority]++));

				//sendBuffer[nextPriority].erase(sendBuffer[nextPriority].begin());

				// Now remove the packet from the list

				if (packet.reliability == PacketReliability::RELIABLE
					|| packet.reliability == PacketReliability::RELIABLE_ORDERED)
				{
					pCurrentPacket = pReliableDatagramPacket;
				}
				else
				{
					pCurrentPacket = pUnrealiableDatagramPacket;
				}

				auto sendPacket = [&]()
				{
					pCurrentPacket->Serialize(bitStream);

					if (m_pSocket)
						m_pSocket->Send(m_RemoteSocketAddress, bitStream.Data(), bitStream.Size());


					if (pCurrentPacket == pReliableDatagramPacket)
					{
						// Add the packet to the resend buffer
						resendBuffer.push_back({curTime, std::unique_ptr<DatagramPacket>(pCurrentPacket)});

						pReliableDatagramPacket = new DatagramPacket();

						pReliableDatagramPacket->header.isACK = false;
						pReliableDatagramPacket->header.isNACK = false;
						pReliableDatagramPacket->header.isReliable = true;
						pReliableDatagramPacket->header.sequenceNumber = flowControlHelper.GetSequenceNumber();
					}
					else
					{
						delete pUnrealiableDatagramPacket;
						pUnrealiableDatagramPacket = new DatagramPacket();

						pUnrealiableDatagramPacket->header.isACK = false;
						pUnrealiableDatagramPacket->header.isNACK = false;
						pUnrealiableDatagramPacket->header.isReliable = false;
					}

					bitStream.Reset();

					// Update the current packet again because it could be used again later
					if (packet.reliability == PacketReliability::RELIABLE
						|| packet.reliability == PacketReliability::RELIABLE_ORDERED)
					{
						pCurrentPacket = pReliableDatagramPacket;
					}
					else
					{
						pCurrentPacket = pUnrealiableDatagramPacket;
					}
				};

				if (packet.GetSizeToSend() + pCurrentPacket->GetSizeToSend() >= MAX_MTU_SIZE)
				{
					if (pCurrentPacket->packets.size() > 0)
						sendPacket();

					if (packet.GetSizeToSend() >= MAX_MTU_SIZE - pCurrentPacket->header.GetSizeToSend())
					{
						// The packet is bigger than MAX_MTU_SIZE so we have to split it
						SplitPacket(packet, &pReliableDatagramPacket);
					}
					else
					{
						// 
						// This packet does not exceed max size, so add it to the next packet

						pCurrentPacket->packets.push_back(std::move(packet));
					}
				}
				else
				{
					// We can add the packet because it will fit in the current packet
					pCurrentPacket->packets.push_back(std::move(packet));
				}

				if (pCurrentPacket->GetSizeToSend() >= MAX_MTU_SIZE)
				{
					sendPacket();
				}

			}

			for (auto p = PacketPriority::LOW; p < PacketPriority::IMMEDIATE; p = (PacketPriority)(p+1))
			{
				if (bufIndex[p] > 0)
				{
					sendBuffer[p].erase(sendBuffer[p].begin(), sendBuffer[p].begin() + (bufIndex[p]));
				}
			}


			if (pUnrealiableDatagramPacket->packets.size())
			{
				bitStream.Reset();
				pUnrealiableDatagramPacket->Serialize(bitStream);
				m_pSocket->Send(m_RemoteSocketAddress, bitStream.Data(), bitStream.Size());

				// Unrealiable packets are not needed anymore
				delete pUnrealiableDatagramPacket;
			}
			else
			{
				delete pUnrealiableDatagramPacket;
			}

			if (pReliableDatagramPacket->packets.size())
			{
				bitStream.Reset();

				pReliableDatagramPacket->Serialize(bitStream);

				m_pSocket->Send(m_RemoteSocketAddress, bitStream.Data(), bitStream.Size());

				resendBuffer.push_back({curTime, std::unique_ptr<DatagramPacket>(pReliableDatagramPacket)});
			}
			else
			{
				delete pReliableDatagramPacket;
			}

		}
	}

	void CReliabilityLayer::RemoveRemote(const SocketAddress& remoteAddress)
	{
		for(const auto& remote : remoteList)
		{
			if(remote.address == remoteAddress)
			{

				remoteList.erase(std::find(remoteList.begin(), remoteList.end(), remote));
				remoteList.shrink_to_fit();

				return;
			}
		}
	}

	bool CReliabilityLayer::ProcessPacket(InternalRecvPacket *pPacket)
	{
		for (auto &remoteSystem : remoteList)
		{
			if (remoteSystem.address == pPacket->remoteAddress)
			{
				CBitStream bitStream{(unsigned char*)pPacket->data, pPacket->bytesRead, true};

				//DEBUG_LOG("Process %d bytes", pPacket->bytesRead);

				// Handle Packet in Reliability Layer
				DatagramPacket dPacket;
				dPacket.Deserialze(bitStream);

				if (dPacket.header.isACK)
				{
					// Handle ACK Packet
					

					uint32 count = 0;
					bitStream.Read(count);

					std::vector<std::pair<int, int>> ranges(count);

					int max;
					int min;

					for (int i = 0; i < count; ++i)
					{
						bitStream.Read(min);
						bitStream.Read(max);

#if DEBUG_ACKS
						DEBUG_LOG("Got ACK for %d %d on {%p}", min, max, this);
#endif

						ranges.push_back({min, max});
					}

					auto size = resendBuffer.size();

					for (int i = size-1; i >= 0; --i)
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

							resendBuffer.erase(resendBuffer.begin() + i);
							//DEBUG_LOG("Remove %d from resend list", i);
						}
					}

					resendBuffer.shrink_to_fit();
				}
				else if (dPacket.header.isNACK)
				{
					// Handle NACK Packet
					// Immediatly resend the packets
				}
				else
				{
					// Now process the packet

#if DEBUG_ACKS
#if 0
					for (auto i : acknowledgements)
					{
						if (dPacket.header.sequenceNumber == i)
						{
							DEBUG_LOG("Already in ACK list %d", i);
							break;
						}
					}
#endif
#endif

					//DEBUG_LOG("Got packet %d", dh.sequenceNumber);

					if (dPacket.header.isReliable)
						acknowledgements.push_back(dPacket.header.sequenceNumber);
					else
						DEBUG_LOG("Got Unrealiable");


					if (dPacket.header.isSplit)
					{
						auto &packet = dPacket.packets[0];

						// handle split packets

						if (packet.splitInfo.index == 0)
						{
							// Check if this index is already in use
							if(splitPacketBuffer.at(packet.splitInfo.packetIndex).size() > 0)
							{
								// Handle already in use

							}
							else
								// first packet so it a new packet
								splitPacketBuffer.at(packet.splitInfo.packetIndex).push_back(std::move(packet));
						}
						else
						{
							splitPacketBuffer.at(packet.splitInfo.packetIndex).push_back(std::move(packet));
						}

						if (packet.splitInfo.isEnd)
						{
							//
							CBitStream bitStream{MAX_MTU_SIZE};

							std::sort(splitPacketBuffer[packet.splitInfo.packetIndex].begin(), splitPacketBuffer[packet.splitInfo.packetIndex].end(), [](const ReliablePacket &packet, const ReliablePacket &packet_) -> bool
							{
								return (packet.splitInfo.index < packet_.splitInfo.index);
							});

							for (auto &tPacket : splitPacketBuffer[packet.splitInfo.packetIndex])
							{
								bitStream.Write(tPacket.Data(), tPacket.Size());
							}

							ReliablePacket completePacket{bitStream.Data(), bitStream.Size()};

							completePacket.orderedInfo = splitPacketBuffer[packet.splitInfo.packetIndex].begin()->orderedInfo;
							completePacket.reliability = splitPacketBuffer[packet.splitInfo.packetIndex].begin()->reliability;

							DEBUG_LOG("Got Packet with %d and %d", completePacket.Size(), strlen(completePacket.Data()));

							splitPacketBuffer[packet.splitInfo.packetIndex].clear();
							splitPacketBuffer[packet.splitInfo.packetIndex].shrink_to_fit();

							if (completePacket.reliability == PacketReliability::RELIABLE_ORDERED)
							{
								completePacket.sequenceNumber = dPacket.header.sequenceNumber;
								completePacket.socketAddress = pPacket->remoteAddress;

								orderedPacketBuffer[packet.orderedInfo.channel].push_back(std::move(completePacket));
							}
							else
							{
								//if (eventHandler)
								{
									eventHandler.Call<ReliablePacket &, SocketAddress&>(ReliabilityEvents::HANDLE_PACKET, completePacket, pPacket->remoteAddress);
								}
							}
						}
					}
					else
					{
						// Handle not split packet
						for (auto &packet : dPacket.packets)
						{
							// 
							if (packet.reliability >= PacketReliability::RELIABLE)
							{
								if (firstUnsentAck.time_since_epoch().count() == 0 || firstUnsentAck == firstUnsentAck.min())
									firstUnsentAck = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
							}

							if (packet.reliability == PacketReliability::RELIABLE_ORDERED)
							{
								packet.sequenceNumber = dPacket.header.sequenceNumber;
								packet.socketAddress = pPacket->remoteAddress;

								orderedPacketBuffer[packet.orderedInfo.channel].push_back(std::move(packet));
							}
							else
							{
								//if(eventHandler)
								{
									eventHandler.Call<ReliablePacket &, SocketAddress&>(ReliabilityEvents::HANDLE_PACKET, packet, pPacket->remoteAddress);
								}
							}
						}
					}
				}

				return true;
			}
		}


		// New connection
		// Handle new connection event
		//if (eventHandler)
		{
			// Handle new connection
			auto r = eventHandler.Call(ReliabilityEvents::NEW_CONNECTION, pPacket);
			if (r != eventHandler.NO_EVENT && r != eventHandler.ALL_TRUE /* one handler refused the connection */)
			{
				// NOTIFY REMOTE SYSTEM THAT THE CONNECTION WAS REFUSED
				// Now call connection refused event

				// Return false because the packet was not handled
				return false;
			}
		}

		RemoteSystem system;

		system.pSocket = pPacket->pSocket;
		system.address = pPacket->remoteAddress;
		remoteList.push_back(std::move(system));

		// Now handle the packet

		ProcessPacket(pPacket);

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

		// This seems to cause a bug sometimes
		// But only a this position, we do it below and everything works fine
		// I still dont know why
		// I have tested this on Windows with Visual Studio 2013 Visual C++ Compiler Nov 2013 (CTP) and there it seems to work fine,
		// so it seems only broken on Linux/GCC
		//acknowledgements.shrink_to_fit();

#if 0
		bool bnil = false;
		for(auto i : acknowledgements)
		{
			if(i == 0)
			{
				bnil = true;
				DEBUG_LOG("on {%p} contains 0", this);
			}
		}
#endif

		// Sort the unsorted vector so we can write the ranges
		std::sort(acknowledgements.begin(), acknowledgements.end());

#if 0
		if(acknowledgements[0] == 1)
			DEBUG_LOG("0 1 on {%p}", this);

		for(auto i : acknowledgements)
		{
			if(i == 0)
			{
				bnil = true;
				DEBUG_LOG("double check on {%p} contains 0", this);
			}
		}
#endif

		CBitStream bitStream{MAX_MTU_SIZE};

		int min = -1;
		int max = 0;
		int writtenTo = 0;
		uint32 writeCount = 0;

		// Now write the range stuff to the bitstream
		for (int i = 0; i < acknowledgements.size(); ++i)
		{
			if ((i+1 < acknowledgements.size()) && acknowledgements[i] == (acknowledgements[i + 1] - 1))
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

#if DEBUG_ACKS
				DEBUG_LOG("Send acks for %d %d", min, max);
#endif

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

#if DEBUG_ACKS
				DEBUG_LOG("Send acks for %d %d", min, max);
#endif

				bitStream.Write<SequenceNumberType>(min);
				bitStream.Write<SequenceNumberType>(max);

				// Track the index we have written to
				writtenTo = i;
				writeCount++;

				min = -1;
			}

			if (bitStream.Size() >= 1300)
			{
				rerun = true;
				break;
			}
		}

		// Remove all acknowledgements we have written from the vector
		acknowledgements.erase(acknowledgements.begin(), acknowledgements.begin() + writtenTo);

		DatagramHeader dh;
		dh.isACK = true;
		dh.isNACK = false;

		CBitStream ackBS{bitStream.Size() + dh.GetSizeToSend()};

		dh.Serialize(ackBS);

		ackBS.Write(writeCount);

		// Write the bitstream with the ack ranges to the bitstream we have to send
		ackBS.Write(bitStream.Data(), bitStream.Size());

		// 
		if (m_pSocket)
			m_pSocket->Send(m_RemoteSocketAddress, ackBS.Data(), ackBS.Size());

		// Reset the firstUnsentAck to 0
		firstUnsentAck = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(std::chrono::milliseconds(0));

		// Check if we have to rerun the SendACKs if we exceeded the max size we can sent in one packet
		if (rerun)
			SendACKs();
		else /* Now shrink the vector so it will not take much memory */
		{
			if(acknowledgements.capacity() > 65535)
				acknowledgements.shrink_to_fit();
		}
	}

	void CReliabilityLayer::SetOrderingChannel(uint8 ucChannel)
	{

	}

	uint8 CReliabilityLayer::GetOrderingChannel()
	{
		return uint8();
	}

	bool CReliabilityLayer::SplitPacket(ReliablePacket &packet, DatagramPacket ** ppDatagramPacket)
	{
		auto curTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());

		// Save the original packet which was given as a param
		// 
		auto pDatagramPacket = *ppDatagramPacket;

		// Split the data in multiple packets
		std::vector<ReliablePacket> splitPackets;
		splitPackets.reserve(packet.Size() / (MAX_MTU_SIZE - pDatagramPacket->header.GetSizeToSend() - 20) + 1);

		int dataOffset = (MAX_MTU_SIZE - pDatagramPacket->header.GetSizeToSend() - 20) + 1;

		uint16 splitIndex = 0;

		uint16 splitPacketNumber = flowControlHelper.GetSplitPacketIndex();

		for (int chunkSize = (MAX_MTU_SIZE - pDatagramPacket->header.GetSizeToSend() - 20) + 1; dataOffset < packet.Size();)
		{

			ReliablePacket tmpPacket(packet.Data() + dataOffset - chunkSize, chunkSize);

			// Set up packet 
			// We need index to merge them together on the remote side
			// 
			// It works similar to the ordered stuff

			tmpPacket.splitInfo.index = splitIndex++;
			tmpPacket.splitInfo.packetIndex = splitPacketNumber;
			tmpPacket.isSplit = true;

			if (tmpPacket.splitInfo.index == 0)
			{
				tmpPacket.reliability = packet.reliability;
				tmpPacket.orderedInfo = packet.orderedInfo;
			}
			
			// In case of Ordered packets only the first packet need the ordered info
			// This reduces the overhead

			if (tmpPacket.splitInfo.index > 0)
				tmpPacket.reliability = PacketReliability::RELIABLE;

			splitPackets.push_back(std::move(tmpPacket));

			if (packet.Size() - (dataOffset) > (MAX_MTU_SIZE - pDatagramPacket->header.GetSizeToSend() - 20) + 1)
			{
				// increase the offset by the size of the next chunk
				dataOffset += chunkSize;

				// and recalculate the chunk size
				chunkSize = (MAX_MTU_SIZE - pDatagramPacket->header.GetSizeToSend() - 20) + 1;
			}
			else
			{
				chunkSize = packet.Size() - dataOffset;
				dataOffset += chunkSize;

				ReliablePacket tmpPacket(packet.Data() + dataOffset - chunkSize, chunkSize);

				// Set up packet 
				// We need index to merge them together on the remote side
				// 
				// It works similar to the ordered stuff

				tmpPacket.isSplit = true;
				tmpPacket.splitInfo.index = splitIndex++;
				tmpPacket.splitInfo.packetIndex = splitPacketNumber;


				// In case of Ordered packets only the first packet need the ordered info
				// This reduces the overhead

				if (tmpPacket.splitInfo.index > 0)
					tmpPacket.reliability = PacketReliability::RELIABLE;

				splitPackets.push_back(std::move(tmpPacket));
			}
		}

		if (dataOffset != packet.Size())
		{
			ReliablePacket tmpPacket(packet.Data() + dataOffset, packet.Size() - dataOffset);

			tmpPacket.isSplit = true;
			tmpPacket.splitInfo.index = splitIndex++;
			tmpPacket.splitInfo.packetIndex = splitPacketNumber;

			if (tmpPacket.splitInfo.index > 0)
				tmpPacket.reliability = PacketReliability::RELIABLE;

			splitPackets.push_back(std::move(tmpPacket));
		}

		splitPackets.back().splitInfo.isEnd = true;

		// Now we have the data split in multiple packets
		// We can now send the packets
		// Each packet will get it´s own Datagram Packet

		DatagramPacket * pSplitDatagramPacket = nullptr;

		// Just in case
		if (pDatagramPacket->packets.size())
		{
			pSplitDatagramPacket = new DatagramPacket();

			pSplitDatagramPacket->header.isACK = false;
			pSplitDatagramPacket->header.isNACK = false;
			pSplitDatagramPacket->header.isReliable = true;
			pSplitDatagramPacket->header.isSplit = true;
			pSplitDatagramPacket->header.sequenceNumber = flowControlHelper.GetSequenceNumber();
		}
		else
		{
			// Use the reliable packet data

			pSplitDatagramPacket = pDatagramPacket;
			pSplitDatagramPacket->header.isSplit = true;
			pDatagramPacket = nullptr;
		}

		CBitStream bitStream{MAX_MTU_SIZE};

		for (auto &splitPacket : splitPackets)
		{
			// Add the packet to the datagram packet
			pSplitDatagramPacket->packets.push_back(std::move(splitPacket));

			// Now send the packet

			pSplitDatagramPacket->Serialize(bitStream);

			if (m_pSocket)
				m_pSocket->Send(m_RemoteSocketAddress, bitStream.Data(), bitStream.Size());

			// Reset the bitstream so we can use it in the next iteration

			bitStream.Reset();

			resendBuffer.push_back({curTime, std::unique_ptr<DatagramPacket>(pSplitDatagramPacket)});

			pSplitDatagramPacket = new DatagramPacket();

			pSplitDatagramPacket->header.isACK = false;
			pSplitDatagramPacket->header.isNACK = false;
			pSplitDatagramPacket->header.isReliable = true;
			pSplitDatagramPacket->header.isSplit = true;
			pSplitDatagramPacket->header.sequenceNumber = flowControlHelper.GetSequenceNumber();
		}

		pSplitDatagramPacket->header.isSplit = false;

		// Assing the nex created datagram packet so the given packet can be used | ah fuck i dont know how to explain this better
		*ppDatagramPacket = pSplitDatagramPacket;

		return true;
	};
};


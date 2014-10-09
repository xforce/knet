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

namespace knet
{
	static const std::chrono::milliseconds resendTime = std::chrono::milliseconds(10000);

	ReliabilityLayer::ReliabilityLayer(ISocket * pSocket)
		: m_pSocket(pSocket)
	{
		firstUnsentAck = firstUnsentAck.min();
		lastReceiveFromRemote = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());

		for(uint32 i = 0; i < orderingIndex.size(); ++i)
		{
			orderingIndex[i] = 0;
		}

		for (uint32 i = 0; i < lastOrderedIndex.size(); ++i)
		{
			lastOrderedIndex[i] = 0;
		}

		highestSequencedReadIndex.fill(0);

		resendBuffer.reserve(512);
	}

	ReliabilityLayer::~ReliabilityLayer()
	{
		
	}

	bool ReliabilityLayer::OnReceive(InternalRecvPacket *packet)
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

	InternalRecvPacket* ReliabilityLayer::PopBufferedPacket()
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

	void ReliabilityLayer::SetTimeout(const std::chrono::milliseconds &time)
	{
		m_msTimeout = time;
	}

	const std::chrono::milliseconds& ReliabilityLayer::GetTimeout() const
	{
		return m_msTimeout;
	}

	void ReliabilityLayer::Send(const char *data, size_t numberOfBitsToSend, PacketPriority priority, PacketReliability reliability)
	{
		if (priority == PacketPriority::IMMEDIATE)
		{
			BitStream bitStream{BITS_TO_BYTES(numberOfBitsToSend) + 20};

			// Just send the packet
			DatagramPacket* pDatagramPacket = new DatagramPacket;

			pDatagramPacket->header.isACK = false;
			pDatagramPacket->header.isNACK = false;

			if (reliability == PacketReliability::RELIABLE
				|| reliability == PacketReliability::RELIABLE_ORDERED
				|| reliability == PacketReliability::RELIABLE_SEQUENCED)
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
			else if (reliability == PacketReliability::RELIABLE_SEQUENCED
				|| reliability == PacketReliability::UNRELIABLE_SEQUENCED)
			{
				sendPacket.sequenceInfo.index = sequencingIndex[orderingChannel]++;
				sendPacket.sequenceInfo.channel = orderingChannel;
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
			else if (reliability == PacketReliability::RELIABLE_SEQUENCED
				|| reliability == PacketReliability::UNRELIABLE_SEQUENCED)
			{
				sendPacket.sequenceInfo.index = sequencingIndex[orderingChannel]++;
				sendPacket.sequenceInfo.channel = orderingChannel;
			}

			sendBuffer[sendPacket.priority].push_back(std::move(sendPacket));

			return;
		}
	}

	void ReliabilityLayer::Process()
	{
		auto curTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());

		if(m_pSocket)
		{
			// Check for timeout
			if(((curTime - lastReceiveFromRemote).count() >= 5000))
			{
				DEBUG_LOG("Disconnect: Timeout");
				// NOW send disconnect event so it will be removed in our peer
				// The Peer which handles this event should stop listening for this socket by calling StopReceiving
				// Then the peer should remove the Remote stuff on next process not in this handler
				eventHandler.Call(ReliabilityEvents::CONNECTION_LOST_TIMEOUT, m_RemoteSocketAddress, DisconnectReason::TIMEOUT);

				return;
			}
		}

		if ((firstUnsentAck.min() == firstUnsentAck) && ((curTime - firstUnsentAck).count() >= 500))
			SendACKs();

		// TODO: process all network and buffered stuff
		InternalRecvPacket * pPacket = nullptr;
		while ((pPacket = PopBufferedPacket()) != nullptr && pPacket != nullptr)
		{
			// TODO: handle return
			ProcessPacket(pPacket, curTime);

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

	void ReliabilityLayer::ProcessResend(std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> &curTime)
	{
		BitStream bitStream{MAX_MTU_SIZE};

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

				// set the time the packet was sent
				resendPacket.first = curTime;

				DEBUG_LOG("Resent packet %d on {%p}", resendPacket.second->header.sequenceNumber, this);
			}
		}
	}

	void ReliabilityLayer::ProcessOrderedPackets(std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> &curTime)
	{
		UNREFERENCED_PARAMETER(curTime);

		int i = 0;
		uint16 lastIndex = 0;
		uint8 channel = 0;

		for (auto & orderedPackets : orderedPacketBuffer)
		{
			if (orderedPackets.size())
			{

				//DEBUG_LOG("Sort");

				// Sort the packets but keep the relative position
				std::stable_sort(orderedPackets.begin(), orderedPackets.end(), [](const ReliablePacket& packet, const ReliablePacket& packet_) -> bool
				{
					return (packet.sequenceNumber < packet_.sequenceNumber);
				});

				if (orderedPackets.size())
				{
					lastIndex = lastOrderedIndex[orderedPackets.begin()->orderedInfo.channel];

					for (auto &packet : orderedPackets)
					{

						if (firstUnsentAck == firstUnsentAck.min())
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

	void ReliabilityLayer::ProcessSend(std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> &curTime)
	{
		BitStream bitStream{MAX_MTU_SIZE};

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


			uint32 bufIndex[PacketPriority::IMMEDIATE] = {0};

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
					|| packet.reliability == PacketReliability::RELIABLE_ORDERED
					|| packet.reliability == PacketReliability::RELIABLE_SEQUENCED)
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
						resendBuffer.push_back(std::move(std::make_pair(curTime, std::unique_ptr<DatagramPacket>(pCurrentPacket))));

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
						|| packet.reliability == PacketReliability::RELIABLE_ORDERED
						|| packet.reliability == PacketReliability::RELIABLE_SEQUENCED)
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

	void ReliabilityLayer::RemoveRemote(const SocketAddress& remoteAddress)
	{
		for(const auto& remote : remoteList)
		{
			if(remote.address == remoteAddress)
			{
				remoteList.erase(std::next(std::find(remoteList.rbegin(), remoteList.rend(), remote)).base());
				remoteList.shrink_to_fit();
				return;
			}
		}
	}

	bool IsOlderOrderedPacket(SequenceNumberType newPacketOrderingIndex, SequenceNumberType waitingForPacketOrderingIndex)
	{
		SequenceNumberType maxRange = (SequenceNumberType) (const uint32_t) -1;

		if (waitingForPacketOrderingIndex > maxRange / 2)
		{
			if (newPacketOrderingIndex >= waitingForPacketOrderingIndex - maxRange / 2 + 1 
				&& newPacketOrderingIndex < waitingForPacketOrderingIndex)
			{
				return true;
			}
		}

		else
		{
			if (newPacketOrderingIndex >= (waitingForPacketOrderingIndex - (maxRange / 2 + 1)) 
				|| newPacketOrderingIndex < waitingForPacketOrderingIndex)
			{
				return true;
			}
		}
		// Old packet
		return false;
	}

	bool ReliabilityLayer::ProcessPacket(InternalRecvPacket *pPacket, std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> &curTime)
	{
		for (auto &remoteSystem : remoteList)
		{
			if (remoteSystem.address == pPacket->remoteAddress)
			{
				BitStream bitStream{(unsigned char*)pPacket->data, pPacket->bytesRead, true};

				//DEBUG_LOG("Process %d bytes", pPacket->bytesRead);

				// TODO: split this in functions

				// Handle Packet in Reliability Layer
				DatagramPacket dPacket;
				dPacket.Deserialze(bitStream);

				if (dPacket.header.isACK)
				{
					// Handle ACK Packet
					uint32 count = 0;
					bitStream.Read(count);

					std::vector<std::pair<int32, int32>> ranges(count);

					int32 max;
					int32 min;

					for (uint32 i = 0; i < count; ++i)
					{
						bitStream.Read(min);
						bitStream.Read(max);

#if DEBUG_ACKS
						DEBUG_LOG("Got ACK for %d %d on {%p}", min, max, this);
#endif

						ranges.push_back({min, max});
					}


					// Now loop through the whole resend buffer and check if we got an ack for a packet,
					// if we got an ack the paket will be removed from the resend buffer

					auto size = resendBuffer.size();

					for (int i = size-1; i >= 0; --i)
					{
						auto sequenceNumber = resendBuffer[i].second->header.sequenceNumber;
						
						// Checks if the given sequence number is in range of the given acks
						auto isInAckRange = [](decltype(ranges)& vecRange, int32 sequenceNumber) {
							return std::any_of(std::begin(vecRange), std::end(vecRange), [sequenceNumber](const auto &k) {
								return (sequenceNumber >= k.first && sequenceNumber <= k.second);
							});
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
					uint32 count = 0;
					bitStream.Read(count);

					std::vector<std::pair<int32, int32>> ranges(count);

					int32 max;
					int32 min;

					for (uint32 i = 0; i < count; ++i)
					{
						bitStream.Read(min);
						bitStream.Read(max);

#if DEBUG_ACKS
						DEBUG_LOG("Got NACK for %d %d on {%p}", min, max, this);
#endif

						ranges.push_back({min, max});
					}


					auto size = resendBuffer.size();

					for (int i = size-1; i >= 0; --i)
					{
						auto sequenceNumber = resendBuffer[i].second->header.sequenceNumber;
						
						// Checks if the given sequence number is in range of the given acks
						auto isInAckRange = [](decltype(ranges)& vecRange, int32 sequenceNumber) {
							return std::any_of(std::begin(vecRange), std::end(vecRange), [sequenceNumber](const auto &k) {
								return (sequenceNumber >= k.first && sequenceNumber <= k.second);
							});
						};

						if (isInAckRange(ranges, sequenceNumber))
						{
							// TODO: handle congestion control
							// TODO: dont sent the packet immediatly, its better to readd it to the send buffer!?

							// Resend packet
							resendBuffer[i].second->Serialize(bitStream);
				
							if (m_pSocket)
								m_pSocket->Send(m_RemoteSocketAddress, bitStream.Data(), bitStream.Size());

							// set the time the packet was sent
							resendBuffer[i].first = curTime;

							bitStream.Reset();
						}
					}
				}
				else
				{
					// Now process the packet

#if DEBUG_ACKS
					for (auto i : acknowledgements)
					{
						if (dPacket.header.sequenceNumber == i)
						{
							DEBUG_LOG("Already in ACK list %d", i);
							break;
						}
					}
#endif

					if (dPacket.header.isReliable)
						acknowledgements.push_back(dPacket.header.sequenceNumber);


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
							BitStream bitStream{MAX_MTU_SIZE};

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
							else if (packet.reliability == PacketReliability::RELIABLE_SEQUENCED || packet.reliability == PacketReliability::UNRELIABLE_SEQUENCED)
							{
								if (!IsOlderOrderedPacket(packet.sequenceInfo.index, highestSequencedReadIndex[packet.orderedInfo.channel]))
								{
									highestSequencedReadIndex[packet.orderedInfo.channel] = packet.sequenceInfo.index + (SequenceIndexType) 1;
									eventHandler.Call<ReliablePacket &, SocketAddress&>(ReliabilityEvents::HANDLE_PACKET, completePacket, pPacket->remoteAddress);
								}
							}
							else
							{
								eventHandler.Call<ReliablePacket &, SocketAddress&>(ReliabilityEvents::HANDLE_PACKET, completePacket, pPacket->remoteAddress);
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
								if (firstUnsentAck == firstUnsentAck.min())
									firstUnsentAck = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
							}

							if (packet.reliability == PacketReliability::RELIABLE_ORDERED)
							{
								packet.sequenceNumber = dPacket.header.sequenceNumber;
								packet.socketAddress = pPacket->remoteAddress;

								orderedPacketBuffer[packet.orderedInfo.channel].push_back(std::move(packet));
							}
							else if (packet.reliability == PacketReliability::RELIABLE_SEQUENCED || packet.reliability == PacketReliability::UNRELIABLE_SEQUENCED)
							{
								if (!IsOlderOrderedPacket(packet.sequenceInfo.index, highestSequencedReadIndex[packet.orderedInfo.channel]))
								{
									highestSequencedReadIndex[packet.orderedInfo.channel] = packet.sequenceInfo.index + (SequenceIndexType) 1;
									eventHandler.Call<ReliablePacket &, SocketAddress&>(ReliabilityEvents::HANDLE_PACKET, packet, pPacket->remoteAddress);
								}
							}
							else
							{
								eventHandler.Call<ReliablePacket &, SocketAddress&>(ReliabilityEvents::HANDLE_PACKET, packet, pPacket->remoteAddress);
							}
						}
					}
				}

				return true;
			}
		}


		// New connection
		// Handle new connection event
		// Handle new connection
		auto r = eventHandler.Call(ReliabilityEvents::NEW_CONNECTION, pPacket);
		if (r != eventHandler.NO_EVENT && r != eventHandler.ALL_TRUE /* one handler refused the connection */)
		{
			// The remote will be notified in Peer this is not the job of the reliability layer
			// Return false because the packet was not handled
			return false;
		}

		RemoteSystem system;

		system.pSocket = pPacket->pSocket;
		system.address = pPacket->remoteAddress;
		remoteList.push_back(std::move(system));

		// Now handle the packet

		ProcessPacket(pPacket, curTime);

		return true;
	}

	std::shared_ptr<ISocket> ReliabilityLayer::GetSocket() const
	{
		return m_pSocket;
	}

	void ReliabilityLayer::SetSocket(std::shared_ptr<ISocket> pSocket)
	{
		if (m_pSocket == pSocket)
			return;

		m_pSocket = pSocket;
	}

	const SocketAddress & ReliabilityLayer::GetRemoteAddress() const
	{
		return m_RemoteSocketAddress;
	}

	void ReliabilityLayer::SetRemoteAddress(SocketAddress &socketAddress)
	{
		m_RemoteSocketAddress = socketAddress;
	}

	void ReliabilityLayer::SendACKs()
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

		BitStream bitStream{MAX_MTU_SIZE};

		int32 min = -1;
		int32 max = 0;
		int32 writtenTo = 0;
		uint32 writeCount = 0;

		// Now write the range stuff to the bitstream
		for (uint32 i = 0; i < acknowledgements.size(); ++i)
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

				// Track the index we have written to, so we can remove the sent acks from the acknowledegements list
				writtenTo = i;
				writeCount++;

				min = -1;
			}

			// If we have exceeded max packet size mark rerun to true,
			// So we will send all acks
			// Is this a good idea or should we mark it to rerun on next process?!
			if (bitStream.Size() >= 1300)
			{
				rerun = true;
				break;
			}
		}

		// Remove all acknowledgements we have written from the vector
		acknowledgements.erase(acknowledgements.begin(), acknowledgements.begin() + writtenTo+1);

		// Setup the datagram header for the ack packet
		DatagramHeader dh;
		dh.isACK = true;
		dh.isNACK = false;

		BitStream ackBS{bitStream.Size() + dh.GetSizeToSend()};

		// Serialize the datagram header
		dh.Serialize(ackBS);

		// write the count of ack ranges which have been written
		ackBS.Write(writeCount);

		// Write the bitstream with the ack ranges to the bitstream we have to send
		ackBS.Write(bitStream.Data(), bitStream.Size());

		// 
		if (m_pSocket)
			m_pSocket->Send(m_RemoteSocketAddress, ackBS.Data(), ackBS.Size());

		// Check if we have to rerun the SendACKs if we exceeded the max size we can sent in one packet
		if (rerun)
		{
			// we dont reset the firstUnsent ack here so it will immediatly rerun on the next process
			SendACKs();
		}
		else /* Now shrink the vector so it will not take much memory */
		{

			// Reset the firstUnsentAck to 0
			firstUnsentAck = firstUnsentAck.min();

			// Only shrink when its really big
			if(acknowledgements.capacity() > 65535)
				acknowledgements.shrink_to_fit();
		}
	}

	void ReliabilityLayer::SetOrderingChannel(uint8 ucChannel)
	{
		orderingChannel = ucChannel;
	}

	uint8 ReliabilityLayer::GetOrderingChannel() const
	{
		return orderingChannel;
	}

	bool ReliabilityLayer::SplitPacket(ReliablePacket &packet, DatagramPacket ** ppDatagramPacket)
	{
		auto curTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());

		// Save the original packet which was given as a param
		// 
		auto pDatagramPacket = *ppDatagramPacket;

		// Split the data in multiple packets
		std::vector<ReliablePacket> splitPackets;
		splitPackets.reserve(packet.Size() / (MAX_MTU_SIZE - pDatagramPacket->header.GetSizeToSend() - 20) + 1);

		uint32 dataOffset = (MAX_MTU_SIZE - pDatagramPacket->header.GetSizeToSend() - 20) + 1;

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

		BitStream bitStream{MAX_MTU_SIZE};

		// TODO: dont send them all together
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

			// Push the sent packet to the resend buffer
			resendBuffer.push_back({curTime, std::unique_ptr<DatagramPacket>(pSplitDatagramPacket)});

			// Create a new datagram packet and set all the flags (split)
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


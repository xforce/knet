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

namespace keksnl
{
	class Event
	{
	public:
	};

	template<typename... Args>
	class EventN : public Event
	{
		typedef std::tuple<Args...> argTuple;
	private:
		std::function<bool(Args... arg)> callback;
	public:
		EventN(decltype(callback) callback)
			: callback(callback)
		{

		}

		bool Invoke(Args... args)
		{
			if (callback)
				return callback(args...);
			
			return false;
		}

		static EventN * cast(Event * event)
		{
			return static_cast<EventN*>(event);
		}
	};

	//typedef int eventIds;

	template<typename eventIds>
	class EventHandler
	{
	private:
		std::vector<std::pair<eventIds, std::vector<Event*>>> events;

	public:
		enum CallResult : char
		{
			NO_EVENT = 0,
			ALL_FALSE,
			ALL_TRUE,
			BOTH,
			MAX_RESULT,
		};

		void AddEvent(eventIds id, Event* pEvent)
		{
			for (auto& entry : events)
			{
				if (entry.first == id)
				{
					entry.second.push_back(pEvent);
					return;
				}
			}
			events.push_back({id, std::vector<Event*> {pEvent}});
		}

		const std::vector<Event *>& GetEventById(eventIds id)
		{
			for (const auto& entry : events)
			{
				if (entry.first == id)
					return entry.second;
			}

			static std::vector<Event*> emptyVec = std::vector<Event*>();
			return emptyVec;
		};

		template<typename... Args>
		CallResult Call(eventIds id, Args... args)
		{
			for (const auto& entry : events)
			{
				if (entry.first == id)
				{
					CallResult ret = CallResult::MAX_RESULT;

					for (auto &event : entry.second)
					{
						if (EventN<Args...>::cast(event)->Invoke(args...))
						{
							// If nothing was set set it to ALL_TRUE;
							// If all previous was FALSE set it to BOTH

							if (ret == CallResult::MAX_RESULT)
								ret = CallResult::ALL_TRUE;
							else if (ret == CallResult::ALL_FALSE)
								ret = CallResult::BOTH;
						}
						else
						{
							// If all were TRUE or BOTH was set set it to BOTH
							// If nothing was set, then set ti to ALL_FALSE
							if (ret == CallResult::ALL_TRUE || ret == CallResult::BOTH)
								ret = CallResult::BOTH;
							else
								ret = CallResult::ALL_FALSE;
						}
					}
					return ret;
				}
			}
			return NO_EVENT;
		}

		explicit operator bool() const NOEXCEPT
		{
			return !(events.size() == 0);
		};
	};
};
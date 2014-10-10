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

/*
	TODO: improve
*/

#include <cstddef>
#include <functional>
#include <type_traits>

template<int...> struct int_sequence { };

template<int N, int... Is> struct make_int_sequence
: make_int_sequence<N - 1, N - 1, Is...>
{ };


template<int... Is> struct make_int_sequence<0, Is...>
	: int_sequence<Is...>{ };

template<int> // begin with 0 here!
struct placeholder_template
{ };



namespace std
{
	template<int N>
	struct is_placeholder< placeholder_template<N> >
		: integral_constant<int, N + 1> // the one is important
	{ };
}

namespace knet
{
	class Event
	{
	private:
		void * owner = nullptr;
	public:

		void SetOwner(void * owner)
		{
			this->owner = owner;
		}

		void * GetOwner()
		{
			return owner;
		}
	};

	template<typename... Args>
	class EventN : public Event
	{
		using argTuple = std::tuple<Args...>;
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

	template<class F>
	struct function_traits;

	// function pointer
	template<class R, class... Args>
	struct function_traits<R(*)(Args...)> : public function_traits<R(Args...)>
	{ };

	// member function pointer
	template<class C, class R, class... Args>
	struct function_traits<R(C::*)(Args...)> : public function_traits<R(C&, Args...)>
	{ };

	// const member function pointer
	template<class C, class R, class... Args>
	struct function_traits<R(C::*)(Args...) const> : public function_traits<R(C&, Args...)>
	{ };

	// member object pointer
	template<class C, class R>
	struct function_traits<R(C::*)> : public function_traits<R(C&)>
	{ };


	template<typename T>
	struct remove_first_type
	{ };

	template<typename T, typename... Ts>
	struct remove_first_type<std::tuple<T, Ts...>>
	{
		using event = EventN<Ts...>;
	};

		// functor
	template<class F>
	struct function_traits
	{
	    private:
	        using call_type = function_traits<decltype(&F::operator())>;
	    public:
	        using return_type = typename call_type::return_type;
	 
	        static constexpr std::size_t arity = call_type::arity - 1;
	 
	        template <std::size_t N>
	        struct argument
	        {
	            static_assert(N < arity, "error: invalid parameter index.");
	            using type = typename call_type::template argument<N+1>::type;
	        };

	        struct eventT
			{
				using eventType = typename call_type::eventT::eventType;
			};
	};


	template<class R, class... Args>
	struct function_traits<R(Args...)>
	{
	public:
		using argTuple = std::tuple<Args...>;
		using argTup = remove_first_type<argTuple>;

		using return_type = R;

		static constexpr int arity = sizeof...(Args);

		template <int N>
		struct argument
		{
			static_assert(N < arity, "error: invalid parameter index.");
			using type = typename std::tuple_element<N, std::tuple<Args...>>::type;
		};

		struct eventT
		{
			using eventType = typename argTup::event;
		};

		struct eventObj
		{
			using eventType = EventN<Args...>;
		};
	};



	struct bind_helper
	{
		template<class _Rx,
		class... _Ftypes,
		int... Is>
		static auto bind(_Rx(*_Pfx)(_Ftypes...), int_sequence<Is...>)
		{
			return std::bind(_Pfx, placeholder_template<Is>{}...);
		}


		template<class _Rx,
		class... _Ftypes>
		static auto bind(_Rx(*_Pfx)(_Ftypes...))
		{
			return bind<_Rx, _Ftypes...>(_Pfx, make_int_sequence<sizeof...(_Ftypes)>{});
		}
	};


	// TEMPLATE FUNCTION bind (implicit return type)
	template<class _Fun>
		Event *
		mkEventN(_Fun&& _Fx)
		{	// bind a function object
			using Traits = function_traits<_Fun>;
			return new typename Traits::eventT::eventType(_Fx);
		}

	template<class _Rx,
	class... _Ftypes,
	class... _Types>
		Event *
		mkEventN(_Rx(*_Pfx)(_Ftypes...), _Types&&... _Args)
		{	// bind a function pointer
			
			auto bn = bind_helper::bind<_Rx, _Ftypes...>(_Pfx);
			return new EventN<_Ftypes...>(bn);
		}

	struct this_bind_helper
	{
		template<class _Rx,
		class _Farg0,
		class _Types,
		int... Is>
		static auto bind(_Rx _Farg0::* const _Pmd, _Types _this, int_sequence<Is...>)
		{
			return std::bind(_Pmd, _this, placeholder_template<Is>{}...);
		}

		template<int N, class _Rx,
		class _Farg0,
		class _Types>
		static auto bind(_Rx _Farg0::* const _Pmd, _Types _Args)
		{
			return bind<_Rx, _Farg0, _Types>(_Pmd, _Args, make_int_sequence<N - 1>{});
		}
	};

	template<class _Rx,
	class _Farg0,
	class _Types>
		Event *
		mkEventN(_Rx _Farg0::* const _Pmd, _Types&& _Args)
		{	// bind a wrapped member object pointer
			using Traits = function_traits<_Rx _Farg0::*>;
			auto bn = this_bind_helper::bind<Traits::arity, _Rx, _Farg0, _Types>(_Pmd, _Args);
			return new typename Traits::eventT::eventType(bn);
		}


#if 0
	template<class _Ret,
	class _Fun,
	class... _Types>
		typename std::enable_if<!std::is_pointer<_Fun>::value
		&& !std::is_member_pointer<_Fun>::value,
		Event*>::type mkEventN(_Fun&& _Fx, _Types&&... _Args)
		{	// bind a function object
			return nullptr;
		}

	template<class _Ret,
	class _Rx,
	class... _Ftypes,
	class... _Types>
		typename std::enable_if<!std::is_same<_Ret, _Rx>::value,
		Event *>::type
		mkEventN(_Rx(*_Pfx)(_Ftypes...), _Types&&... _Args)
		{	// bind a function pointer
			return nullptr;
		}


	template<class _Ret,
	class _Rx,
	class _Farg0,
	class... _Types>
		typename std::enable_if<!std::is_same<_Ret, _Rx>::value
		&& std::is_member_object_pointer<_Rx _Farg0::* const>::value,
		Event*>::type mkEventN(_Rx _Farg0::* const _Pmd, _Types&&... _Args)
		{
			return nullptr;
		}
#endif
	
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

		void RemoveEventsByOwner(void * owner)
		{
			for(auto &eventCon : events)
			{
				eventCon.second.erase(std::remove_if(eventCon.second.begin(), eventCon.second.end(), [owner ](Event * pEvent) -> bool {
					if(pEvent->GetOwner() == owner)
					{
						delete pEvent;
						return true;
					}
					else
						return false;
				}));
			}
		}

		void AddEvent(eventIds id, Event* pEvent, void * owner = nullptr)
		{
			if (!pEvent)
				return;

			pEvent->SetOwner(owner);

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
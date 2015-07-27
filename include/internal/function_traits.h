/*
* Copyright (C) 2014-2015 Crix-Dev
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

#include <functional>
#include <utility>

// TODO: move this to knet namespace

template<int...> struct int_sequence { };

template<int N, int... Is> struct make_int_sequence
	: make_int_sequence < N - 1, N - 1, Is... >
{ };


template<int... Is> struct make_int_sequence < 0, Is... > : int_sequence < Is... >{};

template<int> // begin with 0 here!
struct placeholder_template
{ };

template<typename F>
struct function_traits;

// function pointer
template<typename R, typename... Args>
struct function_traits<R(*)(Args...)> : public function_traits < R(Args...) >
{};

// member function pointer
template<typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...)> : public function_traits < R(C&, Args...) >
{};

// const member function pointer
template<typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...) const> : public function_traits < R(C&, Args...) >
{};

// member object pointer
template<typename C, typename R>
struct function_traits<R(C::*)> : public function_traits < R(C&) >
{};

// functor
template<typename F>
struct function_traits
{
private:
	using call_type = function_traits < decltype(&F::operator()) >;
public:
	using return_type = typename call_type::return_type;
	static constexpr std::size_t arity = call_type::arity - 1;

	template <std::size_t N>
	struct argument
	{
		static_assert(N < arity, "error: invalid parameter index.");
		using type = typename call_type::template argument<N + 1>::type;
	};

	struct argT
	{
		using args = typename call_type::argT::args;
	};
};


template<typename R, typename... Args>
struct function_traits < R(Args...) >
{
public:
	using argTuple = typename std::tuple<Args...>;
	using return_type = R;

	static constexpr int arity = sizeof...(Args);

	template <int N>
	struct argument
	{
		static_assert(N < arity, "error: invalid parameter index.");
		using type = typename std::tuple_element<N, std::tuple<Args...>>::type;
	};

	struct argT
	{
		using args = argTuple;
	};
};

struct bind_helper
{
	template<typename _Rx,
	typename... _Ftypes,
		int... Is>
		static auto bind(_Rx(*_Pfx)(_Ftypes...), int_sequence<Is...>)
	{
		return std::bind(_Pfx, placeholder_template < Is > {}...);
	}


	template<typename _Rx,
		typename... _Ftypes>
		static auto bind(_Rx(*_Pfx)(_Ftypes...))
	{
		return bind<_Rx, _Ftypes...>(_Pfx, make_int_sequence < sizeof...(_Ftypes) > {});
	}
};

struct this_bind_helper
{
	template<typename _Rx,
		typename _Farg0,
		typename _Types,
		int... Is>
		static auto bind(_Rx _Farg0::* const _Pmd, _Types _this, int_sequence<Is...>)
	{
		return std::bind(_Pmd, _this, placeholder_template < Is > {}...);
	}

	template<int N, typename _Rx,
		typename _Farg0,
		typename _Types>
		static auto bind(_Rx _Farg0::* const _Pmd, _Types _Args)
	{
		return bind<_Rx, _Farg0, _Types>(_Pmd, _Args, make_int_sequence < N - 1 > {});
	}
};

namespace std
{
	template<int N>
	struct is_placeholder< placeholder_template<N> >
		: integral_constant < int, N + 1 > // the one is important
	{};
}

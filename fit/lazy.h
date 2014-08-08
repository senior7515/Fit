/*=============================================================================
    Copyright (c) 2014 Paul Fultz II
    lazy.h
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#ifndef FIT_GUARD_FUNCTION_LAZY_H
#define FIT_GUARD_FUNCTION_LAZY_H

/// lazy
/// ====
/// 
/// Description
/// -----------
/// 
/// The `lazy` function adaptor returns a function object call wrapper for a
/// function. Calling this wrapper is equivalent to invoking the function. It
/// is a simple form of lambda expressions, but is constexpr friendly.
/// 
/// Ultimately, calling `lazy(f)(x)` is the equivalent to calling
/// `std::bind(f, x)` except the lazy version can be called in a constexpr
/// context, as well. The `lazy` adaptor is compatible with `std::bind`, so
/// most of the time `lazy` and `std::bind` can be used interchangeably.
/// However, the `lazy` adaptor won't accept member function pointers, like
/// `std::bind` will.
/// 
/// Synopsis
/// --------
/// 
///     template<class F>
///     lazy_adaptor<F> lazy(F f);
/// 
/// Example
/// -------
/// 
///     auto add = [](auto x, auto y) { return x+y; }
///     auto increment = lazy(add)(_1, 1);
///     assert(increment(5) == 6);
/// 

#include <fit/args.h>
#include <fit/conditional.h>
#include <fit/always.h>
#include <fit/static.h>
#include <fit/invoke.h>
#include <fit/detail/delegate.h>
#include <tuple>
#include <functional>
#include <type_traits>

namespace fit {

namespace detail {

struct placeholder_transformer
{
    template<class T>
    struct transformer
    {
        template<class... Ts>
        constexpr auto operator()(Ts&&... xs) const FIT_RETURNS
        (args<std::is_placeholder<T>::value>(std::forward<Ts>(xs)...));
    };

    template<class T, typename std::enable_if<(std::is_placeholder<T>::value > 0), int>::type = 0>
    constexpr transformer<T> operator()(const T&) const
    {
        return {};
    }
};

struct bind_transformer
{
    template<class T, typename std::enable_if<std::is_bind_expression<T>::value, int>::type = 0>
    constexpr const T& operator()(const T& x) const
    {
        return x;
    }
};

template<class T>
struct is_reference_wrapper
: std::false_type
{};

template<class T>
struct is_reference_wrapper<std::reference_wrapper<T>>
: std::true_type
{};

struct ref_transformer
{
    template<class T, typename std::enable_if<is_reference_wrapper<T>::value, int>::type = 0>
    constexpr auto operator()(T x) const 
    FIT_RETURNS(always_ref(x.get()));
};

struct id_transformer
{
    template<class T>
    constexpr auto operator()(const T& x) const 
    FIT_RETURNS(always(x));
};

static constexpr const conditional_adaptor<placeholder_transformer, bind_transformer, ref_transformer, id_transformer> pick_transformer = {};

template<class T, class... Ts>
constexpr auto lazy_transform(T&& x, Ts&&... xs) FIT_RETURNS
(
    fit::detail::pick_transformer(std::forward<T>(x))(std::forward<Ts>(xs)...)
);

template<class F, class T, int ...N, class... Ts>
constexpr auto lazy_invoke_impl(F f, T && t, seq<N...>, Ts&&... xs) FIT_RETURNS
(
    f(fit::detail::lazy_transform(FIT_AUTO_FORWARD(std::get<N>(t)), std::forward<Ts>(xs)...)...)
);

template<class F, class Sequence, class... Ts>
constexpr auto lazy_invoke(F f, Sequence && t, Ts&&... xs) FIT_RETURNS
(
    detail::lazy_invoke_impl(f, std::forward<Sequence>(t), detail::make_sequence_gens(t), std::forward<Ts>(xs)...)
);

template<class F, class Sequence>
struct lazy_invoker : F
{
    Sequence seq;
    template<class X, class Seq>
    constexpr lazy_invoker(X&& x, Seq&& seq) 
    : F(std::forward<X>(x)), seq(std::forward<Seq>(seq))
    {}

    template<class... Ts>
    constexpr const F& base_function(Ts&&... xs) const
    {
        return always_ref(*this)(xs...);
    }

    template<class... Ts>
    constexpr const Sequence& get_sequence(Ts&&... xs) const
    {
        return seq;
    }

    template<class... Ts>
    constexpr auto operator()(Ts&&... xs) const FIT_RETURNS
    (
        fit::detail::lazy_invoke(this->base_function(xs...), this->get_sequence(xs...), std::forward<Ts>(xs)...)
    );
};

template<class F, class Sequence>
constexpr lazy_invoker<F, Sequence> make_lazy_invoker(F f, const Sequence& seq)
{
    return lazy_invoker<F, Sequence>(f, seq);
}
}

template<class F>
struct lazy_adaptor : F
{
    template<class X>
    constexpr lazy_adaptor(X&& x, FIT_ENABLE_IF_CONVERTIBLE(X, F)) : F(std::forward<X>(x))
    {}
    template<class... Ts>
    constexpr const F& base_function(Ts&&... xs) const
    {
        return always_ref(*this)(xs...);
    }

    template<class... Ts>
    constexpr auto operator()(const Ts&... xs) const FIT_RETURNS
    (
        fit::detail::make_lazy_invoker(this->base_function(xs...), 
            std::tuple<Ts...>(xs...))
    );
    
};

template<class F>
constexpr lazy_adaptor<F> lazy(F f)
{
    return lazy_adaptor<F>(f);
}

}

namespace std {
    template<class F, class Sequence>
    struct is_bind_expression<fit::detail::lazy_invoker<F, Sequence>>
    : std::true_type
    {};
}

#endif
// Copyright (C) 2018-2019 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#include <foonathan/lex/list_production.hpp>

#include <catch.hpp>

#include <foonathan/lex/token_production.hpp>

namespace lex = foonathan::lex;

namespace
{
using test_spec = lex::token_spec<struct comma, struct a>;

struct comma : lex::literal_token<','>
{};
struct a : lex::literal_token<'a'>
{};

template <class TLP, typename Func, std::size_t N>
constexpr auto parse(Func&& f, const char (&str)[N])
{
    lex::tokenizer<test_spec> tokenizer(str);
    return TLP::parse(tokenizer, f);
}

constexpr struct unmatched_t
{
} unmatched;

void verify(lex::parse_result<int> result, unmatched_t)
{
    REQUIRE(result.is_unmatched());
}

void verify(lex::parse_result<int> result, int expected)
{
    REQUIRE(result.is_success());
    REQUIRE(result.value() == expected);
}
} // namespace

TEST_CASE("list_production")
{
    using grammar = lex::grammar<test_spec, struct A, struct P>;
    struct A : lex::token_production<A, grammar, a>
    {};
    struct P : lex::list_production<P, grammar, A, comma>
    {};

    struct visitor
    {
        constexpr A operator()(A, a)
        {
            return {};
        }

        constexpr int operator()(P, A)
        {
            return 1;
        }
        constexpr int operator()(P, int list, A)
        {
            return list + 1;
        }

        constexpr void operator()(lex::unexpected_token<grammar, A, a>,
                                  const lex::tokenizer<test_spec>&)
        {}
    };

    constexpr auto r0 = parse<P>(visitor{}, "");
    verify(r0, unmatched);

    constexpr auto r1 = parse<P>(visitor{}, "a");
    verify(r1, 1);

    constexpr auto r2 = parse<P>(visitor{}, "a,a");
    verify(r2, 2);

    constexpr auto r3 = parse<P>(visitor{}, "a,a,a");
    verify(r3, 3);

    constexpr auto r4 = parse<P>(visitor{}, "a,");
    verify(r4, unmatched);

    constexpr auto r5 = parse<P>(visitor{}, ",a");
    verify(r5, unmatched);
}

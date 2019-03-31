// Copyright (C) 2018-2019 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#ifndef FOONATHAN_LEX_OPERATOR_PRODUCTION_HPP_INCLUDED
#define FOONATHAN_LEX_OPERATOR_PRODUCTION_HPP_INCLUDED

#include <foonathan/lex/grammar.hpp>
#include <foonathan/lex/parse_error.hpp>
#include <foonathan/lex/parse_result.hpp>
#include <foonathan/lex/tokenizer.hpp>

namespace foonathan
{
namespace lex
{
    namespace operator_rule
    {
        namespace detail
        {
            template <class TLP, class Func>
            using parse_result
                = lex::parse_result<decltype(std::declval<Func>()(callback_result_of<TLP>{}))>;

            //=== operator_spelling ===//
            template <class... Tokens>
            struct operator_spelling
            {
                static_assert(
                    lex::detail::all_of<lex::detail::type_list<Tokens...>, is_token>::value,
                    "operators must be single tokens");

                template <class TokenSpec>
                static constexpr bool match(const token<TokenSpec>& token)
                {
                    // TODO: C++17
                    return (token.is(Tokens{}) || ...);
                }

                template <class Func, class TLP, class TokenSpec>
                static constexpr parse_result<TLP, Func> apply_prefix(
                    Func& f, TLP, const token<TokenSpec>& op, parse_result<TLP, Func>& value)
                {
                    parse_result<TLP, Func> result;

                    // TODO: C++17
                    (void)((op.is(Tokens{})
                                ? (result = lex::detail::
                                       apply_parse_result(f, TLP{}, lex::static_token<Tokens>(op),
                                                          value.template value_or_tag<TLP>()),
                                   true)
                                : false)
                           || ...);

                    return result;
                }

                template <class Func, class TLP, class TokenSpec>
                static constexpr parse_result<TLP, Func> apply_postfix(
                    Func& f, TLP, parse_result<TLP, Func>& value, const token<TokenSpec>& op)
                {
                    parse_result<TLP, Func> result;

                    // TODO: C++17
                    (void)((op.is(Tokens{})
                                ? (result
                                   = lex::detail::apply_parse_result(f, TLP{},
                                                                     value.template value_or_tag<
                                                                         TLP>(),
                                                                     lex::static_token<Tokens>(op)),
                                   true)
                                : false)
                           || ...);

                    return result;
                }

                template <class Func, class TLP, class TokenSpec>
                static constexpr parse_result<TLP, Func> apply_binary(Func& f, TLP,
                                                                      parse_result<TLP, Func>& lhs,
                                                                      const token<TokenSpec>&  op,
                                                                      parse_result<TLP, Func>& rhs)
                {
                    parse_result<TLP, Func> result;

                    // TODO: C++17
                    (void)((op.is(Tokens{})
                                ? (result = lex::detail::
                                       apply_parse_result(f, TLP{},
                                                          lhs.template value_or_tag<TLP>(),
                                                          lex::static_token<Tokens>(op),
                                                          rhs.template value_or_tag<TLP>()),
                                   true)
                                : false)
                           || ...);

                    return result;
                }
            };

            //=== operator parsers ===//
            template <class Parser, class TLP, class TokenSpec, class Func>
            constexpr auto parse(tokenizer<TokenSpec>& tokenizer, Func& f)
                -> parse_result<TLP, Func>
            {
                auto lhs = Parser::template parse_null<TLP>(tokenizer, f);
                if (lhs.is_unmatched())
                    return lhs;

                return Parser::template parse_left<TLP>(tokenizer, f, lhs);
            }

            template <class Production>
            struct atom
            {
                static_assert(is_production<Production>::value, "atom must be a production");

                template <class TLP, class TokenSpec, class Func>
                static constexpr auto parse_null(tokenizer<TokenSpec>& tokenizer, Func& f)
                    -> parse_result<TLP, Func>
                {
                    auto value = Production::parse(tokenizer, f);
                    if (value.is_unmatched())
                        return {};
                    else
                        return lex::detail::apply_parse_result(f, TLP{},
                                                               value.template value_or_tag<
                                                                   Production>());
                }

                template <class TLP, class TokenSpec, class Func>
                static constexpr auto parse_left(tokenizer<TokenSpec>&, Func&,
                                                 const parse_result<TLP, Func>& lhs)
                {
                    return lhs;
                }
            };

            template <class TokenOpen, class TokenClose, class Atom>
            struct parenthesized
            {
                static_assert(is_token<TokenOpen>::value && is_token<TokenClose>::value,
                              "parentheses must be tokens");

                template <class TLP, class TokenSpec, class Func>
                static constexpr auto parse_null(tokenizer<TokenSpec>& tokenizer, Func& f)
                    -> parse_result<TLP, Func>
                {
                    if (tokenizer.peek().is(TokenOpen{}))
                    {
                        tokenizer.bump();

                        auto value = TLP::parse(tokenizer, f);
                        if (value.is_unmatched())
                            return {};

                        if (tokenizer.peek().is(TokenClose{}))
                            tokenizer.bump();
                        else
                        {
                            auto error = unexpected_token<typename TLP::grammar, TLP,
                                                          TokenClose>(TLP{}, TokenClose{});
                            lex::detail::report_error(f, error, tokenizer);
                            return {};
                        }

                        return value;
                    }
                    else
                        return Atom::template parse_null<TLP>(tokenizer, f);
                }

                template <class TLP, class TokenSpec, class Func>
                static constexpr auto parse_left(tokenizer<TokenSpec>&, Func&,
                                                 const parse_result<TLP, Func>& lhs)
                {
                    return lhs;
                }
            };

            enum associativity
            {
                single,
                left,
                right,
            };

            template <associativity Assoc, class Operator, class Operand>
            struct prefix_op
            {
                template <class TLP, class TokenSpec, class Func>
                static constexpr auto parse_null(tokenizer<TokenSpec>& tokenizer, Func& f)
                    -> parse_result<TLP, Func>
                {
                    auto op = tokenizer.peek();
                    if (Operator::match(op))
                    {
                        tokenizer.bump();

                        auto operand = Assoc == single ? parse<Operand, TLP>(tokenizer, f)
                                                       : parse<prefix_op, TLP>(tokenizer, f);
                        if (operand.is_unmatched())
                            return operand;

                        return Operator::apply_prefix(f, TLP{}, op, operand);
                    }
                    else
                        return Operand::template parse_null<TLP>(tokenizer, f);
                }

                template <class TLP, class TokenSpec, class Func>
                static constexpr auto parse_left(tokenizer<TokenSpec>& tokenizer, Func& f,
                                                 parse_result<TLP, Func>& lhs)
                    -> parse_result<TLP, Func>
                {
                    return Operand::template parse_left<TLP>(tokenizer, f, lhs);
                }
            };

            template <associativity Assoc, class Operator, class Operand>
            struct postfix_op
            {
                template <class TLP, class TokenSpec, class Func>
                static constexpr auto parse_null(tokenizer<TokenSpec>& tokenizer, Func& f)
                    -> parse_result<TLP, Func>
                {
                    return Operand::template parse_null<TLP>(tokenizer, f);
                }

                template <class TLP, class TokenSpec, class Func>
                static constexpr auto parse_left(tokenizer<TokenSpec>& tokenizer, Func& f,
                                                 parse_result<TLP, Func>& lhs)
                    -> parse_result<TLP, Func>
                {
                    lhs = Operand::template parse_left<TLP>(tokenizer, f, lhs);
                    if (lhs.is_unmatched())
                        return lhs;

                    while (Operator::match(tokenizer.peek()))
                    {
                        auto op = tokenizer.get();
                        lhs     = Operator::apply_postfix(f, TLP{}, lhs, op);

                        if (Assoc == single)
                            break;
                    }

                    return lhs;
                }
            };

            template <associativity Assoc, class Operator, class Operand>
            struct binary_op
            {
                template <class TLP, class TokenSpec, class Func>
                static constexpr auto parse_null(tokenizer<TokenSpec>& tokenizer, Func& f)
                    -> parse_result<TLP, Func>
                {
                    return Operand::template parse_null<TLP>(tokenizer, f);
                }

                template <class TLP, class TokenSpec, class Func>
                static constexpr auto parse_left(tokenizer<TokenSpec>& tokenizer, Func& f,
                                                 parse_result<TLP, Func>& lhs)
                    -> parse_result<TLP, Func>
                {
                    lhs = Operand::template parse_left<TLP>(tokenizer, f, lhs);
                    if (lhs.is_unmatched())
                        return lhs;

                    while (Operator::match(tokenizer.peek()))
                    {
                        auto op = tokenizer.get();

                        auto operand = Assoc == right ? parse<binary_op, TLP>(tokenizer, f)
                                                      : parse<Operand, TLP>(tokenizer, f);
                        if (operand.is_unmatched())
                            return operand;

                        lhs = Operator::apply_binary(f, TLP{}, lhs, op, operand);
                        if (Assoc == single && Operator::match(op))
                            break;
                    }

                    return lhs;
                }
            };

            template <class Child>
            struct rule
            {
                template <class TLP, class TokenSpec, class Func>
                static constexpr auto parse(tokenizer<TokenSpec>& tokenizer, Func& f)
                {
                    return detail::parse<Child, TLP>(tokenizer, f);
                }
            };

            template <class Child>
            struct make_rule_impl
            {
                using type = rule<Child>;
            };
            template <class Child>
            struct make_rule_impl<rule<Child>>
            {
                using type = rule<Child>;
            };
            template <class Rule>
            using make_rule = typename make_rule_impl<Rule>::type;
        } // namespace detail

        template <class Production>
        constexpr auto atom = detail::atom<Production>{};

        /// \exclude
        template <class TokenOpen, class TokenClose>
        struct parenthesized_t
        {};

        template <class TokenOpen, class TokenClose>
        constexpr parenthesized_t<TokenOpen, TokenClose> parenthesized{};

        template <class Production, class TokenOpen, class TokenClose>
        constexpr auto operator/(detail::atom<Production>, parenthesized_t<TokenOpen, TokenClose>)
        {
            return detail::parenthesized<TokenOpen, TokenClose, detail::atom<Production>>{};
        }

        template <class... Operator, class Operand>
        constexpr auto pre_op_single(Operand)
        {
            return detail::prefix_op<detail::single, detail::operator_spelling<Operator...>,
                                     Operand>{};
        }
        template <class... Operator, class Operand>
        constexpr auto pre_op_chain(Operand)
        {
            return detail::prefix_op<detail::right, detail::operator_spelling<Operator...>,
                                     Operand>{};
        }

        template <class... Operator, class Operand>
        constexpr auto post_op_single(Operand)
        {
            return detail::postfix_op<detail::single, detail::operator_spelling<Operator...>,
                                      Operand>{};
        }
        template <class... Operator, class Operand>
        constexpr auto post_op_chain(Operand)
        {
            return detail::postfix_op<detail::right, detail::operator_spelling<Operator...>,
                                      Operand>{};
        }

        template <class... Operator, class Operand>
        constexpr auto bin_op_single(Operand)
        {
            return detail::binary_op<detail::single, detail::operator_spelling<Operator...>,
                                     Operand>{};
        }
        template <class... Operator, class Operand>
        constexpr auto bin_op_left(Operand)
        {
            return detail::binary_op<detail::left, detail::operator_spelling<Operator...>,
                                     Operand>{};
        }
        template <class... Operator, class Operand>
        constexpr auto bin_op_right(Operand)
        {
            return detail::binary_op<detail::right, detail::operator_spelling<Operator...>,
                                     Operand>{};
        }
    } // namespace operator_rule

    template <class Derived, class Grammar>
    class operator_production : public detail::base_production
    {
        template <class Func>
        static constexpr auto parse_impl(int, tokenizer<typename Grammar::token_spec>& tokenizer,
                                         Func&& f)
            -> operator_rule::detail::parse_result<Derived, Func>
        {
            using rule = operator_rule::detail::make_rule<decltype(Derived::rule())>;
            return rule::template parse<Derived>(tokenizer, f);
        }

        template <class Func>
        static constexpr auto parse_impl(short, tokenizer<typename Grammar::token_spec>&, Func&&)
        {
            return detail::missing_callback_result_of<Func, Derived>{};
        }

    public:
        using grammar = Grammar;

        template <class Func>
        static constexpr auto parse(tokenizer<typename Grammar::token_spec>& tokenizer, Func&& f)
            -> decltype(parse_impl(0, tokenizer, f))
        {
            return parse_impl(0, tokenizer, f);
        }
    };
} // namespace lex
} // namespace foonathan

#endif // FOONATHAN_LEX_OPERATOR_PRODUCTION_HPP_INCLUDED

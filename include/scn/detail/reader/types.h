// Copyright 2017 Elias Kosunen
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file is a part of scnlib:
//     https://github.com/eliaskosunen/scnlib

#ifndef SCN_DETAIL_READER_TYPES_H
#define SCN_DETAIL_READER_TYPES_H

#include "int.h"

namespace scn {
    SCN_BEGIN_NAMESPACE
    namespace detail {
        struct char_scanner : common_parser {
            static constexpr bool skip_preceding_whitespace()
            {
                return false;
            }

            template <typename ParseCtx>
            error parse(ParseCtx& pctx)
            {
                using char_type = typename ParseCtx::char_type;

                auto c_flag = detail::ascii_widen<char_type>('c');
                bool c_set{};
                return parse_common(pctx, span<const char_type>{&c_flag, 1},
                                    span<bool>{&c_set, 1}, null_each<ParseCtx>);
            }

            template <typename Context>
            error scan(typename Context::char_type& val, Context& ctx)
            {
                auto ch = read_char(ctx.range());
                if (!ch) {
                    return ch.error();
                }
                val = ch.value();
                return {};
            }
        };

        struct buffer_scanner : public common_parser {
            template <typename ParseCtx>
            error parse(ParseCtx& pctx)
            {
                return parse_common(pctx, {}, {}, null_each<ParseCtx>);
            }

            template <typename Context>
            error scan(span<typename Context::char_type>& val, Context& ctx)
            {
                using char_type = typename Context::char_type;

                if (val.size() == 0) {
                    return {};
                }

                auto s = read_zero_copy(ctx.range(), val.ssize());
                if (!s) {
                    return s.error();
                }
                if (s.value().size() != 0) {
                    if (s.value().size() != val.size()) {
                        return {error::end_of_range, "EOF"};
                    }
                    std::memcpy(val.begin(), s.value().begin(),
                                s.value().size() * sizeof(char_type));
                    return {};
                }

                small_vector<char_type, 32> buf(val.size());
                auto it = buf.begin();
                auto e = read_into(ctx.range(), it, val.ssize());
                if (!e) {
                    return e;
                }
                std::memcpy(val.begin(), buf.begin(),
                            buf.size() * sizeof(char_type));
                return {};
            }
        };

        struct bool_scanner : common_parser {
            template <typename ParseCtx>
            error parse(ParseCtx& pctx)
            {
                using char_type = typename ParseCtx::char_type;

                array<char_type, 3> options{{
                    // Only strings
                    ascii_widen<char_type>('s'),
                    // Only ints
                    ascii_widen<char_type>('i'),
                    // Localized digits
                    ascii_widen<char_type>('n'),
                }};
                bool flags[3] = {false};
                auto e = parse_common(
                    pctx, span<const char_type>{options.begin(), options.end()},
                    span<bool>{flags, 3}, null_each<ParseCtx>);

                if (!e) {
                    return e;
                }

                format_options = 0;
                // default ('s' + 'i')
                if (!flags[0] && !flags[1]) {
                    format_options |= allow_string | allow_int;
                }
                // 's'
                if (flags[0]) {
                    format_options |= allow_string;
                }
                // 'i'
                if (flags[1]) {
                    format_options |= allow_int;
                }
                // 'n'
                if (flags[2]) {
                    format_options |= localized_digits;
                    // 'n' implies 'L'
                    common_options |= localized;
                }
                return {};
            }

            template <typename Context>
            error scan(bool& val, Context& ctx)
            {
                using char_type = typename Context::char_type;

                if ((format_options & allow_string) != 0) {
                    auto truename = ctx.locale().get_static().truename();
                    auto falsename = ctx.locale().get_static().falsename();
                    if ((common_options & localized) != 0) {
                        truename = ctx.locale().get_localized().truename();
                        falsename = ctx.locale().get_localized().falsename();
                    }
                    const auto max_len =
                        detail::max(truename.size(), falsename.size());
                    std::basic_string<char_type> buf;
                    buf.reserve(max_len);

                    auto tmp_it = std::back_inserter(buf);
                    auto is_space_pred = make_is_space_predicate(
                        ctx.locale(), (common_options & localized) != 0);
                    auto e = read_until_space(ctx.range(), tmp_it,
                                              is_space_pred, false);
                    if (!e) {
                        return e;
                    }

                    bool found = false;
                    if (buf.size() >= falsename.size()) {
                        if (std::equal(falsename.begin(), falsename.end(),
                                       buf.begin())) {
                            val = false;
                            found = true;
                        }
                    }
                    if (!found && buf.size() >= truename.size()) {
                        if (std::equal(truename.begin(), truename.end(),
                                       buf.begin())) {
                            val = true;
                            found = true;
                        }
                    }
                    if (found) {
                        return {};
                    }
                    else {
                        auto pb =
                            putback_n(ctx.range(),
                                      static_cast<std::ptrdiff_t>(buf.size()));
                        if (!pb) {
                            return pb;
                        }
                    }
                }

                if ((format_options & allow_int) != 0) {
                    if ((format_options & localized_digits) != 0) {
                        int i{};
                        auto s = integer_scanner<int>{};
                        s.common_options = integer_scanner<int>::localized;
                        s.format_options =
                            integer_scanner<int>::only_unsigned |
                            integer_scanner<int>::localized_digits;
                        auto e = s.scan(i, ctx);
                        if (!e) {
                            return e;
                        }
                        if (SCN_UNLIKELY(i != 0 && i != 1)) {
                            return {
                                error::invalid_scanned_value,
                                "Scanned integral boolean not equal to 0 or 1"};
                        }
                        else if (i == 0) {
                            val = false;
                        }
                        else {
                            val = true;
                        }
                        return {};
                    }

                    auto tmp = read_char(ctx.range());
                    if (!tmp) {
                        return tmp.error();
                    }
                    if (tmp.value() == detail::ascii_widen<char_type>('0')) {
                        val = false;
                        return {};
                    }
                    if (tmp.value() == detail::ascii_widen<char_type>('1')) {
                        val = true;
                        return {};
                    }
                    auto pb = putback_n(ctx.range(), 1);
                    if (!pb) {
                        return pb;
                    }
                }

                return {error::invalid_scanned_value, "Couldn't scan bool"};
            }

            enum format_options_type {
                // 's' option
                allow_string = 1,
                // 'i' option
                allow_int = 2,
                // 'n' option
                localized_digits = 4
            };
            uint8_t format_options{allow_string | allow_int};
        };

    }  // namespace detail
    SCN_END_NAMESPACE
}  // namespace scn

#endif

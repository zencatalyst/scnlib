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

#ifndef SCN_DETAIL_UTF8_H
#define SCN_DETAIL_UTF8_H

#include "locale.h"
#include "string_view.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    using code_point = uint32_t;

    namespace detail {
        enum : uint16_t {
            lead_surrogate_min = 0xd800,
            lead_surrogate_max = 0xdbff,
            trail_surrogate_min = 0xdc00,
            trail_surrogate_max = 0xdfff,
            lead_offset = lead_surrogate_min - (0x10000 >> 10),
        };
        enum : uint32_t {
            surrogate_offset =
                0x10000u - (lead_surrogate_min << 10) - trail_surrogate_min,
            code_point_max = 0x10ffff
        };

        template <typename Octet>
        constexpr uint8_t mask8(Octet o)
        {
            return static_cast<uint8_t>(0xff & o);
        }
        template <typename U16>
        constexpr uint16_t mask16(U16 v)
        {
            return static_cast<uint16_t>(0xffff & v);
        }

        template <typename Octet>
        constexpr bool is_trail(Octet o)
        {
            return (mask8(o) >> 6) == 2;
        }
        template <typename U16>
        constexpr bool is_lead_surrogate(U16 cp)
        {
            return cp >= lead_surrogate_min && cp <= lead_surrogate_max;
        }
        template <typename U16>
        constexpr bool is_trail_surrogate(U16 cp)
        {
            return cp >= trail_surrogate_min && cp <= trail_surrogate_max;
        }
        template <typename U16>
        constexpr bool is_surrogate(U16 cp)
        {
            return cp >= lead_surrogate_min && cp <= trail_surrogate_max;
        }

        constexpr inline bool is_code_point_valid(code_point cp)
        {
            return cp <= code_point_max && !is_surrogate(cp);
        }
    }  // namespace detail

    constexpr inline code_point make_code_point(char ch)
    {
        return static_cast<code_point>(ch);
    }

    constexpr inline bool is_entire_code_point(code_point cp)
    {
        return detail::is_code_point_valid(cp);
    }
    template <typename Octet>
    SCN_CONSTEXPR14 int get_sequence_length(Octet ch)
    {
        uint8_t lead = detail::mask8(ch);
        if (lead < 0x80) {
            return 1;
        }
        else if ((lead >> 5) == 6) {
            return 2;
        }
        else if ((lead >> 4) == 0xe) {
            return 3;
        }
        else if ((lead >> 3) == 0x1e) {
            return 4;
        }
        return 0;
    }

    namespace detail {
        SCN_CONSTEXPR14 inline bool is_overlong_sequence(code_point cp,
                                                         std::ptrdiff_t len)
        {
            if (cp < 0x80) {
                if (len != 1) {
                    return true;
                }
            }
            else if (cp < 0x800) {
                if (len != 2) {
                    return true;
                }
            }
            else if (cp < 0x10000) {
                if (len != 3) {
                    return true;
                }
            }

            return false;
        }

        template <typename I, typename S>
        SCN_CONSTEXPR14 error increase_safely(I& it, S end)
        {
            if (++it == end) {
                return {error::invalid_encoding,
                        "Unexpected end of range when decoding utf8 "
                        "(partial codepoint)"};
            }
            if (!is_trail(*it)) {
                return {error::invalid_encoding,
                        "Invalid utf8 codepoint parsed"};
            }
            return {};
        }

        template <typename I, typename S>
        SCN_CONSTEXPR14 error get_sequence_1(I& it, S end, code_point& cp)
        {
            SCN_EXPECT(it != end);
            cp = mask8(*it);
            return {};
        }
        template <typename I, typename S>
        SCN_CONSTEXPR14 error get_sequence_2(I& it, S end, code_point& cp)
        {
            SCN_EXPECT(it != end);
            cp = mask8(*it);

            auto e = increase_safely(it, end);
            if (!e) {
                return e;
            }

            cp = ((cp << 6) & 0x7ff) + ((*it) & 0x3f);

            return {};
        }
        template <typename I, typename S>
        SCN_CONSTEXPR14 error get_sequence_3(I& it, S end, code_point& cp)
        {
            SCN_EXPECT(it != end);
            cp = mask8(*it);

            auto e = increase_safely(it, end);
            if (!e) {
                return e;
            }

            cp = ((cp << 12) & 0xffff) + ((mask8(*it) << 6) & 0xfff);

            e = increase_safely(it, end);
            if (!e) {
                return e;
            }

            cp += (*it) & 0x3f;

            return {};
        }
        template <typename I, typename S>
        SCN_CONSTEXPR14 error get_sequence_4(I& it, S end, code_point& cp)
        {
            SCN_EXPECT(it != end);
            cp = mask8(*it);

            auto e = increase_safely(it, end);
            if (!e) {
                return e;
            }

            cp = ((cp << 18) & 0x1fffff) + ((mask8(*it) << 12) & 0x3ffff);

            e = increase_safely(it, end);
            if (!e) {
                return e;
            }

            cp += (mask8(*it) << 6) & 0xfff;

            e = increase_safely(it, end);
            if (!e) {
                return e;
            }

            cp += (*it) & 0x3f;

            return {};
        }

        template <typename I, typename S>
        SCN_CONSTEXPR14 error validate_next(I& it, S end, code_point& cp)
        {
            SCN_EXPECT(it != end);

            int len = get_sequence_length(*it);
            error e{};
            switch (len) {
                case 0:
                    return {error::invalid_encoding,
                            "Invalid lead byte for utf8"};
                case 1:
                    e = get_sequence_1(it, end, cp);
                    break;
                case 2:
                    e = get_sequence_2(it, end, cp);
                    break;
                case 3:
                    e = get_sequence_3(it, end, cp);
                    break;
                case 4:
                    e = get_sequence_4(it, end, cp);
                    break;
            }

            if (!e) {
                return e;
            }
            if (!is_code_point_valid(cp) || is_overlong_sequence(cp, len)) {
                return {error::invalid_encoding, "Invalid utf8 code point"};
            }
            ++it;
            return {};
        }
    }  // namespace detail

    template <typename I, typename S>
    SCN_CONSTEXPR14 expected<I> parse_code_point(I begin, S end, code_point& cp)
    {
        code_point c{};
        auto e = detail::validate_next(begin, end, c);
        if (e) {
            cp = c;
            return {begin};
        }
        return e;
    }

    SCN_END_NAMESPACE
}  // namespace scn

#endif

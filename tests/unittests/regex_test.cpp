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

#include "wrapped_gtest.h"

#include <scn/detail/regex.h>
#include <scn/detail/scan.h>

using namespace std::string_view_literals;

TEST(RegexTest, String)
{
    auto r = scn::scan<std::string>("foobar123", "{:/([a-zA-Z]+)/}");
    ASSERT_TRUE(r);
    EXPECT_FALSE(r->range().empty());
    EXPECT_EQ(r->value(), "foobar");
}

TEST(RegexTest, StringView)
{
    auto r =
        scn::scan<std::string_view>("foobar123", "{:/([a-zA-Z]+)/}");
    ASSERT_TRUE(r);
    EXPECT_FALSE(r->range().empty());
    EXPECT_EQ(r->value(), "foobar");
}

TEST(RegexTest, Matches)
{
    auto r =
        scn::scan<scn::regex_matches>("foobar123", "{:/([a-zA-Z]+)([0-9]+)/}");
    ASSERT_TRUE(r);
    EXPECT_TRUE(r->range().empty());
    EXPECT_THAT(r->value().matches,
                testing::ElementsAre(
                    testing::Optional(testing::Property(
                        &scn::regex_matches::match::get, "foobar123"sv)),
                    testing::Optional(testing::Property(
                        &scn::regex_matches::match::get, "foobar"sv)),
                    testing::Optional(testing::Property(
                        &scn::regex_matches::match::get, "123"sv))));
}

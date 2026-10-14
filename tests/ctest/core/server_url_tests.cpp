// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ServerUrl.h"

#include <gtest/gtest.h>

TEST(ServerUrl, HostAndPortComeFromTheAuthority)
{
	EXPECT_EQ(ServerUrl::Host("http://127.0.0.1:9550/service/init"), "127.0.0.1");
	EXPECT_EQ(ServerUrl::Port("http://127.0.0.1:9550/service/init"), 9550);
}

TEST(ServerUrl, PortDefaultsTo80)
{
	EXPECT_EQ(ServerUrl::Host("http://eamuse.example/"), "eamuse.example");
	EXPECT_EQ(ServerUrl::Port("http://eamuse.example/"), 80);
}

TEST(ServerUrl, SchemeIsOptional)
{
	EXPECT_EQ(ServerUrl::Host("localhost:9551"), "localhost");
	EXPECT_EQ(ServerUrl::Port("localhost:9551"), 9551);
}

TEST(ServerUrl, ABadPortFallsBackTo80)
{
	EXPECT_EQ(ServerUrl::Port("http://host:notaport/"), 80);
}

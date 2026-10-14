// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DEV9/InetLoopbackPatch.h"

#include <gtest/gtest.h>

#include <array>

namespace
{
	constexpr u32 LOOPBACK_CHECK_INDEX = 11;

	constexpr std::array<u32, 23> SCE_INET_INTERFACE_OUTPUT = {0x3042ff00, 0x00822025, 0x00031e02, 0x12200002, 0x00839025,
		0x8e300070, 0x15000003, 0x3c02ff00, 0x3c087f00, 0x35080001, 0x01021024, 0x3c037f00, 0x10430003, 0x00000000, 0x15100008,
		0x02202021, 0x02202021, 0x02602821, 0x01003021, 0x0c023cd7, 0x02003821, 0x08023dc7, 0x00000000};
}

TEST(InetLoopbackPatch, FindsOnlyTheLoopbackCompare)
{
	EXPECT_EQ(InetLoopbackPatch::FindLoopbackChecks(SCE_INET_INTERFACE_OUTPUT), std::vector<u32>{LOOPBACK_CHECK_INDEX});
}

TEST(InetLoopbackPatch, DisabledCheckKeepsTheRegisterAndNeverMatches)
{
	EXPECT_EQ(InetLoopbackPatch::DisabledLoopbackCheck(SCE_INET_INTERFACE_OUTPUT[LOOPBACK_CHECK_INDEX]), 0x24030001u);
}

TEST(InetLoopbackPatch, PatchedCodeIsNotFoundAgain)
{
	std::array<u32, 23> code = SCE_INET_INTERFACE_OUTPUT;
	code[LOOPBACK_CHECK_INDEX] = InetLoopbackPatch::DisabledLoopbackCheck(code[LOOPBACK_CHECK_INDEX]);
	EXPECT_TRUE(InetLoopbackPatch::FindLoopbackChecks(code).empty());
}

TEST(InetLoopbackPatch, IgnoresACompareWithoutTheFirstOctetMask)
{
	std::array<u32, 23> code = SCE_INET_INTERFACE_OUTPUT;
	code[7] = 0;
	EXPECT_TRUE(InetLoopbackPatch::FindLoopbackChecks(code).empty());
}

// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "USB/usb-python2/Python2BootPatch.h"

#include <gtest/gtest.h>

#include <array>

namespace
{
	constexpr u32 PREFIX_LOAD_INDEX = 9;

	constexpr std::array<u32, 51> DDR_SUPERNOVA_JP_PROFILE_ID = {0x39ce0001, 0x9207000b, 0x262f0104, 0x92080004, 0x262200c3,
		0x9209000a, 0x01ee100b, 0x920b0009, 0x920a0008, 0x240f0040, 0x920c0007, 0x0000182d, 0x920d0006, 0x24460001, 0x920e0005,
		0x27a50080, 0xa3af0081, 0xa3a80082, 0xa3a90083, 0xa3ab0084, 0xa3aa0085, 0xa3ac0086, 0xa3ad0087, 0xa3ae0088, 0xa3a70089,
		0xa3be0080, 0x90ad0000, 0x00037840, 0x01e25821, 0x000d6902, 0x25ae0030, 0x29af000a, 0x15e00002, 0x25ac0037, 0x0180702d,
		0xa16e0000, 0x90af0000, 0x00c0582d, 0x31ed000f, 0x25ae0030, 0x29af000a, 0x15e00002, 0x25ac0037, 0x0180702d, 0x24630001,
		0xa16e0000, 0x24c60002, 0x286f000a, 0x15e0ffe9, 0x24a50001, 0xa0400014};
}

TEST(Python2BootPatch, FindsThePrefixLoadInTheBootloader)
{
	EXPECT_EQ(Python2BootPatch::FindPcbIdPrefixLoads(DDR_SUPERNOVA_JP_PROFILE_ID, 0x12), std::vector<u32>{PREFIX_LOAD_INDEX});
}

TEST(Python2BootPatch, FindsTheLoadAgainAfterItWasPatched)
{
	std::array<u32, 51> code = DDR_SUPERNOVA_JP_PROFILE_ID;
	code[PREFIX_LOAD_INDEX] = 0x240f0012;
	EXPECT_EQ(Python2BootPatch::FindPcbIdPrefixLoads(code, 0x12), std::vector<u32>{PREFIX_LOAD_INDEX});
}

TEST(Python2BootPatch, IgnoresALoadWithoutTheHexLoop)
{
	std::array<u32, 51> code = DDR_SUPERNOVA_JP_PROFILE_ID;
	code[30] = code[39] = 0;
	EXPECT_TRUE(Python2BootPatch::FindPcbIdPrefixLoads(code, 0x12).empty());
}

TEST(Python2BootPatch, IgnoresALoadThatIsNotStoredBesideTheHeader)
{
	std::array<u32, 51> code = DDR_SUPERNOVA_JP_PROFILE_ID;
	code[25] = 0;
	EXPECT_TRUE(Python2BootPatch::FindPcbIdPrefixLoads(code, 0x12).empty());
}

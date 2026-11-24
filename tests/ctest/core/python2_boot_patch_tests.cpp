// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "USB/usb-python2/Python2BootPatch.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace
{
	constexpr u32 PREFIX_LOAD_INDEX = 9;
	constexpr u32 EE_RAM_WORDS = 0x2000000 / 4;
	constexpr u32 LIVE_FUNCTION = 0x1800b60;
	constexpr u32 FILE_IMAGE_FUNCTION = 0x4e8fc0;
	constexpr u32 PROFILE_ID_OFFSET_IN_FUNCTION = 0x128;
	constexpr u32 PROFILE_ARGUMENT_OFFSET_IN_FUNCTION = 0x34;
	constexpr u32 FUNCTION_PROLOGUE = 0x27bdff20;
	constexpr u32 MOVE_PROFILE_FROM_A0 = 0x0080882d;
	constexpr u32 PROFILE_GETTER = 0x100540;
	constexpr std::array<u32, 3> PROFILE_GETTER_BODY = {0x3c02002c, 0x03e00008, 0x2442f140};
	constexpr u32 GETTER_CALL = 0x1801250;
	constexpr std::array<u32, 3> GETTER_CALL_WORDS = {0x0c040150, 0x00000000, 0x0040a82d};
	constexpr u32 DONGLE_READER_CALL = 0x18012f4;
	constexpr std::array<u32, 2> DONGLE_READER_CALL_WORDS = {0x0c6002d8, 0x02a0202d};
	constexpr u32 SUPERNOVA2_SYSID = 0x2bf244;

	constexpr u32 CODE_ENGINE = 0x80188;
	constexpr std::array<u32, 21> CODE_ENGINE_DISPATCH = {0x3c0b0008, 0x8d6b0824, 0x116000db, 0x240c0000, 0x3c080008, 0x25080828,
		0x3c090008, 0x25290508, 0x000c08c0, 0x01015021, 0x8d440000, 0x108000ce, 0x00043702, 0x00063080, 0x00c93021, 0x8ccd0000,
		0x11a000c9, 0x000421c0, 0x000421c2, 0x01a00008, 0x8d450004};
	constexpr u32 CODE_LIST_COUNT = 0x80824;
	constexpr std::array<u32, 27> MIDNIGHT_SPECIAL_SUPERNOVA2_CODES = {0x0000000d, 0x202bf1f0, 0x474b4700, 0x202bf1f4, 0x41554a44,
		0x202bf1f8, 0x44470041, 0x202bf1fc, 0x41552d4a, 0xd19d3c58, 0x0101000c, 0x019d3c58, 0x00000000, 0xd02bf216, 0x01000000,
		0x01990db8, 0x00000000, 0x202bf244, 0x39393939, 0x202bf248, 0x32393939, 0x202bf24c, 0x33373633, 0x202bf250, 0x36303435,
		0x202bf254, 0x32313330};

	constexpr std::array<u32, 51> DDR_SUPERNOVA_JP_PROFILE_ID = {0x39ce0001, 0x9207000b, 0x262f0104, 0x92080004, 0x262200c3,
		0x9209000a, 0x01ee100b, 0x920b0009, 0x920a0008, 0x240f0040, 0x920c0007, 0x0000182d, 0x920d0006, 0x24460001, 0x920e0005,
		0x27a50080, 0xa3af0081, 0xa3a80082, 0xa3a90083, 0xa3ab0084, 0xa3aa0085, 0xa3ac0086, 0xa3ad0087, 0xa3ae0088, 0xa3a70089,
		0xa3be0080, 0x90ad0000, 0x00037840, 0x01e25821, 0x000d6902, 0x25ae0030, 0x29af000a, 0x15e00002, 0x25ac0037, 0x0180702d,
		0xa16e0000, 0x90af0000, 0x00c0582d, 0x31ed000f, 0x25ae0030, 0x29af000a, 0x15e00002, 0x25ac0037, 0x0180702d, 0x24630001,
		0xa16e0000, 0x24c60002, 0x286f000a, 0x15e0ffe9, 0x24a50001, 0xa0400014};

	template <size_t N>
	void Place(std::vector<u32>& ram, u32 address, const std::array<u32, N>& words)
	{
		std::copy(words.begin(), words.end(), ram.begin() + address / 4);
	}

	void PlaceDongleReader(std::vector<u32>& ram, u32 function)
	{
		ram[function / 4] = FUNCTION_PROLOGUE;
		ram[(function + PROFILE_ARGUMENT_OFFSET_IN_FUNCTION) / 4] = MOVE_PROFILE_FROM_A0;
		Place(ram, function + PROFILE_ID_OFFSET_IN_FUNCTION, DDR_SUPERNOVA_JP_PROFILE_ID);
	}

	std::vector<u32> SuperNova2BootRam()
	{
		std::vector<u32> ram(EE_RAM_WORDS);
		PlaceDongleReader(ram, LIVE_FUNCTION);
		PlaceDongleReader(ram, FILE_IMAGE_FUNCTION);
		Place(ram, PROFILE_GETTER, PROFILE_GETTER_BODY);
		Place(ram, GETTER_CALL, GETTER_CALL_WORDS);
		Place(ram, DONGLE_READER_CALL, DONGLE_READER_CALL_WORDS);
		Place(ram, CODE_ENGINE, CODE_ENGINE_DISPATCH);
		Place(ram, CODE_LIST_COUNT, MIDNIGHT_SPECIAL_SUPERNOVA2_CODES);
		return ram;
	}

	u32 PrefixLoadIndex(u32 function) { return (function + PROFILE_ID_OFFSET_IN_FUNCTION) / 4 + PREFIX_LOAD_INDEX; }
}

TEST(Python2BootPatch, PatchesOnlyTheDongleReaderThatIsCalled)
{
	const std::vector<u32> ram = SuperNova2BootRam();
	EXPECT_EQ(Python2BootPatch::FindPcbIdPrefixLoads(ram, 0x12).size(), 2u);
	EXPECT_EQ(Python2BootPatch::FindCalledPcbIdPrefixLoads(ram, 0x12), std::vector<u32>{PrefixLoadIndex(LIVE_FUNCTION)});
}

TEST(Python2BootPatch, FindsTheSysidBufferThroughTheProfileGetter)
{
	const std::vector<u32> ram = SuperNova2BootRam();
	EXPECT_EQ(Python2BootPatch::FindPcbIdBufferAddress(ram, PrefixLoadIndex(LIVE_FUNCTION)), SUPERNOVA2_SYSID);
}

TEST(Python2BootPatch, FindsTheMidnightSpecialCodesThatWriteTheSysid)
{
	const std::vector<u32> ram = SuperNova2BootRam();
	const std::vector<u32> expected = {0x80868 / 4, 0x80870 / 4, 0x80878 / 4, 0x80880 / 4, 0x80888 / 4};
	EXPECT_EQ(Python2BootPatch::FindCodeEngineWritesTo(ram, SUPERNOVA2_SYSID, 21), expected);
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

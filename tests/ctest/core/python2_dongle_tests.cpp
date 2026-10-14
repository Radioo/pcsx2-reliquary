// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "USB/usb-python2/Python2Dongle.h"

#include <gtest/gtest.h>

namespace
{
	Python2Dongle::Bytes WhiteDongle(std::initializer_list<u8> head, u8 crc)
	{
		Python2Dongle::Bytes dongle = {};
		std::copy(head.begin(), head.end(), dongle.begin());
		dongle[39] = crc;
		return dongle;
	}

	const Python2Dongle::Bytes DUMPED_WHITE = WhiteDongle({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x43, 0x17, 0xb4, 0x2a,
															  0x3e, 0x87, 0x20, 0x08, 0x82, 0x20, 0x08, 0x82},
		0x94);
	const Python2Dongle::Bytes TESTPCB2_WHITE = WhiteDongle({0x54, 0x45, 0x53, 0x54, 0x50, 0x43, 0x42, 0x32, 0x9e, 0x33, 0xa3, 0xec,
																0x10, 0xad, 0x20, 0x08, 0x82, 0x20, 0x08, 0x82},
		0xca);
	const Python2Dongle::Serial DUMPED_SERIAL = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	const Python2Dongle::Serial TESTPCB2_SERIAL = {0x54, 0x45, 0x53, 0x54, 0x50, 0x43, 0x42, 0x32};
}

TEST(Python2Dongle, BuildWhiteMatchesADumpedWhiteDongle)
{
	EXPECT_EQ(Python2Dongle::BuildWhite(DUMPED_SERIAL), DUMPED_WHITE);
}

TEST(Python2Dongle, BuildWhiteMatchesADongleTheGameAccepted)
{
	EXPECT_EQ(Python2Dongle::BuildWhite(TESTPCB2_SERIAL), TESTPCB2_WHITE);
}

TEST(Python2Dongle, FormatPcbIdMatchesWhatTheGameSent)
{
	EXPECT_EQ(Python2Dongle::FormatPcbId(Python2Dongle::BOOTLOADER_PCB_ID_PREFIX, DUMPED_WHITE), "01400107060504030208");
	EXPECT_EQ(Python2Dongle::FormatPcbId(Python2Dongle::BOOTLOADER_PCB_ID_PREFIX, TESTPCB2_WHITE), "01405442435054534532");
}

TEST(Python2Dongle, ParsePcbIdSplitsThePrefixFromTheSerial)
{
	const std::optional<Python2Dongle::PcbId> pcb_id = Python2Dongle::ParsePcbId("01123456789ABCDEF012");
	ASSERT_TRUE(pcb_id.has_value());
	EXPECT_EQ(pcb_id->prefix, 0x12);
	EXPECT_EQ(pcb_id->serial, (Python2Dongle::Serial{0x34, 0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x12}));
}

TEST(Python2Dongle, ParsedPcbIdFormatsBackUnchanged)
{
	for (const std::string_view text : {"01123456789ABCDEF012", "01400107060504030208", "01405442435054534532"})
	{
		const std::optional<Python2Dongle::PcbId> pcb_id = Python2Dongle::ParsePcbId(text);
		ASSERT_TRUE(pcb_id.has_value());
		EXPECT_EQ(Python2Dongle::FormatPcbId(pcb_id->prefix, Python2Dongle::BuildWhite(pcb_id->serial)), text);
	}
}

TEST(Python2Dongle, ParsePcbIdRejectsMalformedIds)
{
	EXPECT_FALSE(Python2Dongle::ParsePcbId("").has_value());
	EXPECT_FALSE(Python2Dongle::ParsePcbId("01123456789ABCDEF0").has_value());
	EXPECT_FALSE(Python2Dongle::ParsePcbId("01123456789ABCDEF01200").has_value());
	EXPECT_FALSE(Python2Dongle::ParsePcbId("01123456789ABCDEF01G").has_value());
	EXPECT_FALSE(Python2Dongle::ParsePcbId("1G1G1G1G1G1G1G1G1G1G").has_value());
	EXPECT_FALSE(Python2Dongle::ParsePcbId("02123456789ABCDEF012").has_value());
}

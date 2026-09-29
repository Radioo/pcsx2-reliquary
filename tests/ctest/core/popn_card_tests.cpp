// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "FireWire/Devices/PopnCard.h"

#include <gtest/gtest.h>

namespace
{
	PopnCard::Bytes ConvertedPopn10Card()
	{
		PopnCard::Bytes card = {};
		const u8 record[] = {0x68, 0x23, 0x49, 0x01, 0x9f, 0x1f, 0x34, 0xcd, 0x12, 0xab, 0x00, 0x01, 0x04, 0xe0};
		std::copy(std::begin(record), std::end(record), card.begin());
		card[63] = 0xb6;
		card[64] = 0xd1;
		return card;
	}
} // namespace

TEST(PopnCard, DescribesACardPopn10Rewrote)
{
	const PopnCard::Info info = PopnCard::Describe(ConvertedPopn10Card());

	EXPECT_EQ(info.type, PopnCard::Type::Popn10Registered);
	EXPECT_EQ(info.game, PopnCard::Game::Popn10);
	EXPECT_TRUE(info.type_checksum_ok);
	EXPECT_TRUE(info.data_checksum_ok);
	EXPECT_EQ(info.card_data, 0x1f);
	EXPECT_EQ(info.number, "E0040100AB12CD34");
}

TEST(PopnCard, BuildReproducesTheGamesOwnBytes)
{
	EXPECT_EQ(PopnCard::Build(PopnCard::Type::Popn10Registered, 0x1f, "E0040100AB12CD34"), ConvertedPopn10Card());
}

TEST(PopnCard, APopn9RegisteredCardUsesThePopn9Header)
{
	const PopnCard::Bytes card = PopnCard::Build(PopnCard::Type::Popn9Registered, 21, "E0040100AB12CD34");

	EXPECT_EQ(card[0], 0x68);
	EXPECT_EQ(card[2], 0x41);
	EXPECT_EQ(card[4], 0xe9);
	EXPECT_EQ(PopnCard::Describe(card).game, PopnCard::Game::Popn9);
}

TEST(PopnCard, AChangedByteBreaksTheDataChecksumAndRepairFixesIt)
{
	PopnCard::Bytes card = ConvertedPopn10Card();
	card[30] = 0x01;
	EXPECT_FALSE(PopnCard::Describe(card).data_checksum_ok);

	PopnCard::RepairChecksums(card);
	EXPECT_TRUE(PopnCard::Describe(card).data_checksum_ok);
}

TEST(PopnCard, SettingCardDataKeepsTheCardValid)
{
	PopnCard::Bytes card = ConvertedPopn10Card();
	PopnCard::SetCardData(card, 21);

	const PopnCard::Info info = PopnCard::Describe(card);
	EXPECT_EQ(info.card_data, 21);
	EXPECT_TRUE(info.data_checksum_ok);
}

TEST(PopnCard, UnknownHeadersAreReportedAsUnknown)
{
	PopnCard::Bytes card = {};
	card[0] = 0x12;

	EXPECT_EQ(PopnCard::Describe(card).type, PopnCard::Type::Unknown);
}

TEST(PopnCard, NumbersAreNormalized)
{
	EXPECT_EQ(PopnCard::NormalizeNumber("e004 0100 ab12 cd34"), "E0040100AB12CD34");
	EXPECT_EQ(PopnCard::NormalizeNumber("E004-0100-AB12-CD34"), "E0040100AB12CD34");
	EXPECT_FALSE(PopnCard::NormalizeNumber("E0040100AB12CD3").has_value());
	EXPECT_FALSE(PopnCard::NormalizeNumber("E0040100AB12CD3G").has_value());
}

TEST(PopnCard, RandomNumbersAreValidCardNumbers)
{
	const std::string number = PopnCard::RandomNumber();

	EXPECT_EQ(PopnCard::NormalizeNumber(number), number);
	EXPECT_EQ(number.substr(0, 4), "E004");
}

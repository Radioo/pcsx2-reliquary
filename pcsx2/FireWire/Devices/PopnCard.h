// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class SettingsInterface;

namespace PopnCard
{
	static constexpr const char* SETTINGS_SECTION = "Python1/Game";
	static constexpr const char* CARD_FILE_KEY = "CardFile";
	static constexpr u32 FILE_SIZE = 128;
	static constexpr u32 RECORD_SIZE = 65;
	static constexpr u32 TYPE_SIZE = 4;
	static constexpr u32 TYPE_CHECKSUM_OFFSET = 4;
	static constexpr u32 CARD_DATA_OFFSET = 5;
	static constexpr u32 NUMBER_OFFSET = 6;
	static constexpr u32 NUMBER_SIZE = 8;
	static constexpr u32 DATA_CHECKSUM_OFFSET = 63;

	using Bytes = std::array<u8, FILE_SIZE>;

	enum class Type : u8
	{
		KonamiCommon,
		EamusementCommon,
		Popn9Blank,
		Popn9Registered,
		Popn9Death,
		Popn9LocationTestBlank,
		Popn9LocationTestRegistered,
		Popn10Blank,
		Popn10Registered,
		Popn10LocationTestBlank,
		Popn10LocationTestRegistered,
		Unknown,
	};

	enum class Game : u8
	{
		Popn9,
		Popn10,
		Common,
		Unknown,
	};

	struct Info
	{
		Type type;
		Game game;
		bool type_checksum_ok;
		bool data_checksum_ok;
		u8 card_data;
		std::string number;
	};

	Info Describe(const Bytes& card);
	Bytes Build(Type type, u8 card_data, std::string_view number);
	void SetCardData(Bytes& card, u8 value);
	void RepairChecksums(Bytes& card);
	std::optional<std::string> NormalizeNumber(std::string_view text);
	std::string RandomNumber();

	std::string GetWalletDirectory();
	std::string GetCardPath(std::string_view name);
	std::vector<std::string> ListCards();
	std::string UniqueName(std::string_view base);
	std::optional<Bytes> Load(const std::string& path);
	bool Save(const std::string& path, const Bytes& card);

	bool AdoptLegacySettings(SettingsInterface& sif, std::string_view serial);
} // namespace PopnCard

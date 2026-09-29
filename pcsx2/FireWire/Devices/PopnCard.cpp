// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "FireWire/Devices/PopnCard.h"

#include "Config.h"

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SettingsInterface.h"
#include "common/StringUtil.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <random>

namespace PopnCard
{
	namespace
	{
		constexpr u32 DATA_CHECKSUM_START = CARD_DATA_OFFSET;
		constexpr u32 DATA_CHECKSUM_LENGTH = DATA_CHECKSUM_OFFSET - DATA_CHECKSUM_START;
		constexpr u32 NUMBER_DIGITS = NUMBER_SIZE * 2;
		constexpr u32 RANDOM_NUMBER_DIGITS = 12;
		constexpr std::string_view RANDOM_NUMBER_PREFIX = "E004";
		constexpr std::string_view WALLET_FOLDER = "popn_cards";
		constexpr std::string_view CARD_EXTENSION = ".bin";
		constexpr const char* CARD_PATTERN = "*.bin";
		constexpr const char* LEGACY_NUMBER_KEY = "CardNumber";
		constexpr const char* LEGACY_DESIGN_KEY = "CardDesign";
		constexpr const char* LEGACY_CARD_FILE_FORMAT = "python1_popn_card_%.*s.bin";
		constexpr u32 CARD_DATA_MAX = 0xff;

		struct Template
		{
			Type type;
			Game game;
			std::array<u8, TYPE_SIZE> header;
		};

		constexpr std::array<Template, 11> TEMPLATES = {{
			{Type::KonamiCommon, Game::Common, {0x08, 0x1f, 0x7d, 0xf1}},
			{Type::EamusementCommon, Game::Common, {0x08, 0x20, 0x82, 0x01}},
			{Type::Popn9Blank, Game::Popn9, {0x28, 0x23, 0x41, 0x01}},
			{Type::Popn9Registered, Game::Popn9, {0x68, 0x23, 0x41, 0x01}},
			{Type::Popn9Death, Game::Popn9, {0x6c, 0x23, 0x41, 0x01}},
			{Type::Popn9LocationTestBlank, Game::Popn9, {0x18, 0x23, 0x41, 0x01}},
			{Type::Popn9LocationTestRegistered, Game::Popn9, {0x58, 0x23, 0x41, 0x01}},
			{Type::Popn10Blank, Game::Popn10, {0x28, 0x23, 0x49, 0x01}},
			{Type::Popn10Registered, Game::Popn10, {0x68, 0x23, 0x49, 0x01}},
			{Type::Popn10LocationTestBlank, Game::Popn10, {0x18, 0x23, 0x49, 0x01}},
			{Type::Popn10LocationTestRegistered, Game::Popn10, {0x58, 0x23, 0x49, 0x01}},
		}};

		u8 Crc8(const u8* data, u32 size)
		{
			u8 crc = 0xff;
			for (u32 i = 0; i < size; i++)
			{
				crc ^= data[i];
				for (u32 bit = 0; bit < 8; bit++)
					crc = (crc & 1) ? static_cast<u8>((crc >> 1) ^ 0x8c) : static_cast<u8>(crc >> 1);
			}
			return static_cast<u8>(~crc);
		}

		u16 Crc16(const u8* data, u32 size)
		{
			u16 crc = 0xffff;
			for (u32 i = 0; i < size; i++)
			{
				crc ^= data[i];
				for (u32 bit = 0; bit < 8; bit++)
					crc = (crc & 1) ? static_cast<u16>((crc >> 1) ^ 0x8408) : static_cast<u16>(crc >> 1);
			}
			return static_cast<u16>(~crc);
		}

		u16 StoredDataChecksum(const Bytes& card)
		{
			return static_cast<u16>(card[DATA_CHECKSUM_OFFSET] | (card[DATA_CHECKSUM_OFFSET + 1] << 8));
		}
	} // namespace

	Info Describe(const Bytes& card)
	{
		Info info = {Type::Unknown, Game::Unknown, false, false, card[CARD_DATA_OFFSET], {}};
		for (const Template& entry : TEMPLATES)
		{
			if (std::equal(entry.header.begin(), entry.header.end(), card.begin()))
			{
				info.type = entry.type;
				info.game = entry.game;
				break;
			}
		}

		info.type_checksum_ok = card[TYPE_CHECKSUM_OFFSET] == Crc8(card.data(), TYPE_SIZE);
		info.data_checksum_ok = StoredDataChecksum(card) == Crc16(card.data() + DATA_CHECKSUM_START, DATA_CHECKSUM_LENGTH);
		for (u32 i = 0; i < NUMBER_SIZE; i++)
			info.number += StringUtil::StdStringFromFormat("%02X", card[NUMBER_OFFSET + NUMBER_SIZE - 1 - i]);
		return info;
	}

	Bytes Build(Type type, u8 card_data, std::string_view number)
	{
		Bytes card = {};
		const auto entry = std::find_if(TEMPLATES.begin(), TEMPLATES.end(), [type](const Template& t) { return t.type == type; });
		if (entry != TEMPLATES.end())
			std::copy(entry->header.begin(), entry->header.end(), card.begin());

		card[CARD_DATA_OFFSET] = card_data;
		for (u32 i = 0; i < NUMBER_SIZE && (i * 2 + 1) < number.size(); i++)
		{
			const std::string pair(number.substr(i * 2, 2));
			card[NUMBER_OFFSET + NUMBER_SIZE - 1 - i] = static_cast<u8>(std::strtoul(pair.c_str(), nullptr, 16));
		}

		RepairChecksums(card);
		return card;
	}

	void SetCardData(Bytes& card, u8 value)
	{
		card[CARD_DATA_OFFSET] = value;
		RepairChecksums(card);
	}

	void RepairChecksums(Bytes& card)
	{
		card[TYPE_CHECKSUM_OFFSET] = Crc8(card.data(), TYPE_SIZE);
		const u16 data_crc = Crc16(card.data() + DATA_CHECKSUM_START, DATA_CHECKSUM_LENGTH);
		card[DATA_CHECKSUM_OFFSET] = static_cast<u8>(data_crc);
		card[DATA_CHECKSUM_OFFSET + 1] = static_cast<u8>(data_crc >> 8);
	}

	std::optional<std::string> NormalizeNumber(std::string_view text)
	{
		std::string digits;
		for (const char c : text)
		{
			if (std::isxdigit(static_cast<unsigned char>(c)))
				digits.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
			else if (c != ' ' && c != '-')
				return std::nullopt;
		}

		if (digits.size() != NUMBER_DIGITS)
			return std::nullopt;
		return digits;
	}

	std::string RandomNumber()
	{
		std::random_device device;
		std::uniform_int_distribution<u32> nibble(0, 15);
		std::string number(RANDOM_NUMBER_PREFIX);
		for (u32 i = 0; i < RANDOM_NUMBER_DIGITS; i++)
			number += StringUtil::StdStringFromFormat("%X", nibble(device));
		return number;
	}

	std::string GetWalletDirectory()
	{
		return Path::Combine(EmuFolders::MemoryCards, WALLET_FOLDER);
	}

	std::string GetCardPath(std::string_view name)
	{
		return Path::Combine(GetWalletDirectory(), std::string(name) + std::string(CARD_EXTENSION));
	}

	std::vector<std::string> ListCards()
	{
		FileSystem::FindResultsArray results;
		FileSystem::FindFiles(GetWalletDirectory().c_str(), CARD_PATTERN,
			FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_RELATIVE_PATHS | FILESYSTEM_FIND_SORT_BY_NAME, &results);

		std::vector<std::string> names;
		names.reserve(results.size());
		for (const FILESYSTEM_FIND_DATA& result : results)
			names.emplace_back(Path::GetFileTitle(result.FileName));
		return names;
	}

	std::string UniqueName(std::string_view base)
	{
		std::string name(base);
		for (u32 suffix = 2; FileSystem::FileExists(GetCardPath(name).c_str()); suffix++)
			name = StringUtil::StdStringFromFormat("%.*s %u", static_cast<int>(base.size()), base.data(), suffix);
		return name;
	}

	std::optional<Bytes> Load(const std::string& path)
	{
		const std::optional<std::vector<u8>> data = FileSystem::ReadBinaryFile(path.c_str());
		if (!data.has_value() || data->size() < RECORD_SIZE)
			return std::nullopt;

		Bytes card = {};
		std::copy_n(data->begin(), std::min<size_t>(data->size(), card.size()), card.begin());
		return card;
	}

	bool Save(const std::string& path, const Bytes& card)
	{
		return FileSystem::EnsureDirectoryExists(std::string(Path::GetDirectory(path)).c_str(), true) &&
		       FileSystem::WriteBinaryFile(path.c_str(), card.data(), card.size());
	}

	bool AdoptLegacySettings(SettingsInterface& sif, std::string_view serial)
	{
		if (!sif.ContainsValue(SETTINGS_SECTION, LEGACY_NUMBER_KEY) && !sif.ContainsValue(SETTINGS_SECTION, LEGACY_DESIGN_KEY))
			return false;

		const std::optional<std::string> number = NormalizeNumber(sif.GetStringValue(SETTINGS_SECTION, LEGACY_NUMBER_KEY, ""));
		const std::string design = sif.GetStringValue(SETTINGS_SECTION, LEGACY_DESIGN_KEY, "");
		sif.DeleteValue(SETTINGS_SECTION, LEGACY_NUMBER_KEY);
		sif.DeleteValue(SETTINGS_SECTION, LEGACY_DESIGN_KEY);
		if (!sif.GetStringValue(SETTINGS_SECTION, CARD_FILE_KEY, "").empty())
			return true;

		std::optional<Bytes> card = Load(Path::Combine(EmuFolders::MemoryCards,
			StringUtil::StdStringFromFormat(LEGACY_CARD_FILE_FORMAT, static_cast<int>(serial.size()), serial.data())));
		if (card.has_value() && number.has_value())
		{
			const Info info = Describe(card.value());
			const bool registered = info.type == Type::Popn9Registered || info.type == Type::Popn10Registered;
			if (!info.type_checksum_ok || !info.data_checksum_ok || !registered || info.number != number.value())
				card.reset();
		}
		if (!card.has_value() && number.has_value())
			card = Build(Type::Popn9Registered, 0, number.value());
		if (!card.has_value())
			return true;

		if (!design.empty())
		{
			const u32 value = static_cast<u32>(std::strtoul(design.c_str(), nullptr, 10));
			if (value <= CARD_DATA_MAX)
				SetCardData(card.value(), static_cast<u8>(value));
		}

		const Game game = Describe(card.value()).game;
		const std::string name = UniqueName(game == Game::Popn10 ? "pop'n 10 card" : (game == Game::Popn9 ? "pop'n 9 card" : "Card"));
		if (Save(GetCardPath(name), card.value()))
			sif.SetStringValue(SETTINGS_SECTION, CARD_FILE_KEY, name.c_str());
		return true;
	}
} // namespace PopnCard

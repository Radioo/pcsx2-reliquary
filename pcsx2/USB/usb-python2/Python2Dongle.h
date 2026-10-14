// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace Python2Dongle
{
	static constexpr u32 SIZE = 40;
	static constexpr u32 SERIAL_SIZE = 8;
	static constexpr u8 PCB_ID_HEADER = 0x01;
	static constexpr u8 BOOTLOADER_PCB_ID_PREFIX = 0x40;

	using Bytes = std::array<u8, SIZE>;
	using Serial = std::array<u8, SERIAL_SIZE>;

	struct PcbId
	{
		u8 prefix;
		Serial serial;
	};

	std::optional<PcbId> ParsePcbId(std::string_view text);
	Bytes BuildWhite(const Serial& serial);
	std::string FormatPcbId(u8 prefix, const Bytes& dongle);
}

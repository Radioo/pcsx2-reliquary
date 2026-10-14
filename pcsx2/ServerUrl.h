// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace ServerUrl
{
	using Address = std::array<u8, 4>;

	struct Endpoint
	{
		Address address;
		u16 port;
	};

	std::string Host(std::string_view url);
	u16 Port(std::string_view url);
	std::optional<Address> Resolve(const std::string& host);
}

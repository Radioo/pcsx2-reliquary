// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <optional>
#include <span>
#include <vector>

namespace Python2BootPatch
{
	std::vector<u32> FindPcbIdPrefixLoads(std::span<const u32> code, u8 prefix);
	std::vector<u32> FindCalledPcbIdPrefixLoads(std::span<const u32> ram, u8 prefix);
	std::optional<u32> FindPcbIdBufferAddress(std::span<const u32> ram, u32 prefix_load);
	std::vector<u32> FindCodeEngineWritesTo(std::span<const u32> ram, u32 address, u32 size);
	bool ApplyPcbId(u8 prefix);
}

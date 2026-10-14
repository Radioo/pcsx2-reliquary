// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <span>
#include <vector>

namespace Python2BootPatch
{
	std::vector<u32> FindPcbIdPrefixLoads(std::span<const u32> code, u8 prefix);
	bool ApplyPcbIdPrefix(u8 prefix);
}

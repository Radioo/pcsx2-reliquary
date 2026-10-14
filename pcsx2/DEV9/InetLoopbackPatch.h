// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <span>
#include <vector>

namespace InetLoopbackPatch
{
	std::vector<u32> FindLoopbackChecks(std::span<const u32> code);
	u32 DisabledLoopbackCheck(u32 check);
	void Ensure();
}

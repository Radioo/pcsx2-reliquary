// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DEV9/InetLoopbackPatch.h"

#include "IopMem.h"
#include "R3000A.h"
#include "common/Console.h"

#include <chrono>
#include <optional>

namespace
{
	constexpr u32 OPCODE_SPECIAL = 0x00;
	constexpr u32 OPCODE_BEQ = 0x04;
	constexpr u32 OPCODE_ADDIU = 0x09;
	constexpr u32 OPCODE_LUI = 0x0f;
	constexpr u32 FUNCT_AND = 0x24;
	constexpr u32 LOOPBACK_NETWORK_HIGH = 0x7f00;
	constexpr u32 FIRST_OCTET_MASK_HIGH = 0xff00;
	constexpr u32 MASK_SEARCH_WINDOW = 4;
	constexpr u32 NEVER_A_FIRST_OCTET = 1;
	constexpr u32 RECOMPILED_BLOCK_REACH = 0x400;
	constexpr auto RESCAN_INTERVAL = std::chrono::seconds(1);

	u32 Opcode(u32 word) { return word >> 26; }
	u32 Rs(u32 word) { return (word >> 21) & 0x1f; }
	u32 Rt(u32 word) { return (word >> 16) & 0x1f; }
	u32 Rd(u32 word) { return (word >> 11) & 0x1f; }
	u32 Immediate(u32 word) { return word & 0xffff; }

	bool IsLui(u32 word, u32 immediate)
	{
		return Opcode(word) == OPCODE_LUI && Rs(word) == 0 && Immediate(word) == immediate;
	}

	bool IsLoopbackCheck(std::span<const u32> code, u32 index)
	{
		if (index == 0 || index + 1 >= code.size() || !IsLui(code[index], LOOPBACK_NETWORK_HIGH))
			return false;

		const u32 loopback = Rt(code[index]);
		const u32 branch = code[index + 1];
		if (Opcode(branch) != OPCODE_BEQ || (Rs(branch) != loopback && Rt(branch) != loopback))
			return false;

		const u32 masked = Rs(branch) == loopback ? Rt(branch) : Rs(branch);
		const u32 mask = code[index - 1];
		if (Opcode(mask) != OPCODE_SPECIAL || (mask & 0x3f) != FUNCT_AND || Rd(mask) != masked)
			return false;

		const u32 first = index > MASK_SEARCH_WINDOW ? index - MASK_SEARCH_WINDOW : 0;
		for (u32 candidate = first; candidate < index - 1; candidate++)
		{
			const u32 word = code[candidate];
			if (IsLui(word, FIRST_OCTET_MASK_HIGH) && (Rt(word) == Rs(mask) || Rt(word) == Rt(mask)))
				return true;
		}
		return false;
	}

	struct Patched
	{
		u32 address;
		u32 word;
	};

	std::optional<Patched> s_patched;
	std::chrono::steady_clock::time_point s_last_scan;
}

std::vector<u32> InetLoopbackPatch::FindLoopbackChecks(std::span<const u32> code)
{
	std::vector<u32> out;
	for (u32 index = 0; index < code.size(); index++)
	{
		if (IsLoopbackCheck(code, index))
			out.push_back(index);
	}
	return out;
}

u32 InetLoopbackPatch::DisabledLoopbackCheck(u32 check)
{
	return (OPCODE_ADDIU << 26) | (Rt(check) << 16) | NEVER_A_FIRST_OCTET;
}

void InetLoopbackPatch::Ensure()
{
	if (!iopMem)
		return;

	const std::span<const u32> ram(reinterpret_cast<const u32*>(iopMem->Main), Ps2MemSize::IopRam / sizeof(u32));
	if (s_patched.has_value() && ram[s_patched->address / sizeof(u32)] == s_patched->word)
		return;

	const auto now = std::chrono::steady_clock::now();
	if (now - s_last_scan < RESCAN_INTERVAL)
		return;
	s_last_scan = now;
	s_patched.reset();

	const std::vector<u32> checks = FindLoopbackChecks(ram);
	if (checks.size() != 1)
	{
		if (!checks.empty())
			Console.Error("DEV9: found %zu INET loopback checks, leaving them unpatched", checks.size());
		return;
	}

	const u32 address = checks.front() * sizeof(u32);
	const u32 word = DisabledLoopbackCheck(ram[checks.front()]);
	iopMemWrite32(address, word);
	const u32 first_cleared = address > RECOMPILED_BLOCK_REACH ? address - RECOMPILED_BLOCK_REACH : 0;
	psxCpu->Clear(first_cleared, (address - first_cleared) / sizeof(u32) + 1);
	s_patched = Patched{address, word};
	Console.WriteLn("DEV9: INET loopback check at 0x%06x disabled, 127.x traffic goes to the server", address);
}

// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "USB/usb-python2/Python2BootPatch.h"

#include "USB/usb-python2/Python2Dongle.h"

#include "Memory.h"
#include "common/Console.h"

#include <algorithm>

namespace
{
	constexpr u32 OPCODE_ADDIU = 0x09;
	constexpr u32 OPCODE_SLTI = 0x0a;
	constexpr u32 OPCODE_SB = 0x28;
	constexpr u32 REGISTER_ZERO = 0;
	constexpr u32 REGISTER_SP = 29;
	constexpr u32 IMMEDIATE_MASK = 0xffff;
	constexpr u32 PREFIX_STORE_WINDOW = 16;
	constexpr u32 HEADER_STORE_WINDOW = 24;
	constexpr u32 HEX_LOOP_WINDOW = 48;
	constexpr u32 HEX_DIGIT_BASE = 0x30;
	constexpr u32 HEX_LETTER_BASE = 0x37;
	constexpr u32 HEX_DIGIT_LIMIT = 0x0a;

	u32 Opcode(u32 word) { return word >> 26; }
	u32 Rs(u32 word) { return (word >> 21) & 0x1f; }
	u32 Rt(u32 word) { return (word >> 16) & 0x1f; }
	u32 Immediate(u32 word) { return word & IMMEDIATE_MASK; }

	bool IsImmediateOp(u32 word, u32 opcode, u32 immediate)
	{
		return Opcode(word) == opcode && Immediate(word) == immediate;
	}

	bool IsStackByteStore(u32 word, u32 offset)
	{
		return Opcode(word) == OPCODE_SB && Rs(word) == REGISTER_SP && Immediate(word) == offset;
	}

	bool HasHexLoop(std::span<const u32> window)
	{
		const auto has = [&window](u32 opcode, u32 immediate) {
			return std::any_of(window.begin(), window.end(), [&](u32 word) { return IsImmediateOp(word, opcode, immediate); });
		};
		return has(OPCODE_ADDIU, HEX_DIGIT_BASE) && has(OPCODE_SLTI, HEX_DIGIT_LIMIT) && has(OPCODE_ADDIU, HEX_LETTER_BASE);
	}

	bool IsPcbIdPrefixLoad(std::span<const u32> code, u32 index)
	{
		const u32 word = code[index];
		if (Opcode(word) != OPCODE_ADDIU || Rs(word) != REGISTER_ZERO)
			return false;

		const std::span<const u32> after = code.subspan(index + 1);
		const std::span<const u32> prefix_window = after.first(std::min<size_t>(PREFIX_STORE_WINDOW, after.size()));
		const auto prefix_store = std::find_if(prefix_window.begin(), prefix_window.end(), [&](u32 candidate) {
			return Opcode(candidate) == OPCODE_SB && Rs(candidate) == REGISTER_SP && Rt(candidate) == Rt(word);
		});
		if (prefix_store == prefix_window.end() || Immediate(*prefix_store) == 0)
			return false;

		const u32 header_offset = Immediate(*prefix_store) - 1;
		const std::span<const u32> header_window = after.first(std::min<size_t>(HEADER_STORE_WINDOW, after.size()));
		if (std::none_of(header_window.begin(), header_window.end(), [&](u32 candidate) { return IsStackByteStore(candidate, header_offset); }))
			return false;

		return HasHexLoop(after.first(std::min<size_t>(HEX_LOOP_WINDOW, after.size())));
	}
}

std::vector<u32> Python2BootPatch::FindPcbIdPrefixLoads(std::span<const u32> code, u8 prefix)
{
	std::vector<u32> out;
	for (u32 index = 0; index < code.size(); index++)
	{
		const u32 immediate = Immediate(code[index]);
		if ((immediate == Python2Dongle::BOOTLOADER_PCB_ID_PREFIX || immediate == prefix) && IsPcbIdPrefixLoad(code, index))
			out.push_back(index);
	}
	return out;
}

bool Python2BootPatch::ApplyPcbIdPrefix(u8 prefix)
{
	if (!eeMem)
		return false;

	const std::span<const u32> ram(reinterpret_cast<const u32*>(eeMem->Main), Ps2MemSize::MainRam / sizeof(u32));
	const std::vector<u32> loads = FindPcbIdPrefixLoads(ram, prefix);
	if (loads.size() != 1)
	{
		if (!loads.empty())
			Console.Error("P2IO: found %zu bootloader PCB ID prefix loads, leaving them unpatched", loads.size());
		return false;
	}

	const u32 address = loads.front() * sizeof(u32);
	const u32 patched = (ram[loads.front()] & ~IMMEDIATE_MASK) | prefix;
	if (ram[loads.front()] != patched)
	{
		memWrite32(address, patched);
		Console.WriteLn("P2IO: bootloader PCB ID prefix at 0x%08x set to %02X", address, prefix);
	}
	return true;
}

// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "USB/usb-python2/Python2BootPatch.h"

#include "USB/usb-python2/Python2Dongle.h"

#include "Memory.h"
#include "common/Console.h"

#include <algorithm>
#include <array>

namespace
{
	constexpr u32 OPCODE_SPECIAL = 0x00;
	constexpr u32 OPCODE_JAL = 0x03;
	constexpr u32 OPCODE_BEQ = 0x04;
	constexpr u32 OPCODE_ADDIU = 0x09;
	constexpr u32 OPCODE_SLTI = 0x0a;
	constexpr u32 OPCODE_LUI = 0x0f;
	constexpr u32 OPCODE_LW = 0x23;
	constexpr u32 OPCODE_SB = 0x28;
	constexpr u32 FUNCT_SLL = 0x00;
	constexpr u32 FUNCT_SRL = 0x02;
	constexpr u32 FUNCT_JR = 0x08;
	constexpr u32 FUNCT_MOVN = 0x0b;
	constexpr u32 FUNCT_ADDU = 0x21;
	constexpr u32 FUNCT_DADDU = 0x2d;
	constexpr u32 REGISTER_ZERO = 0;
	constexpr u32 REGISTER_V0 = 2;
	constexpr u32 REGISTER_A0 = 4;
	constexpr u32 REGISTER_SP = 29;
	constexpr u32 REGISTER_RA = 31;
	constexpr u32 IMMEDIATE_MASK = 0xffff;
	constexpr u32 JUMP_TARGET_MASK = 0x03ffffff;
	constexpr u32 PREFIX_STORE_WINDOW = 16;
	constexpr u32 HEADER_STORE_WINDOW = 24;
	constexpr u32 HEX_LOOP_WINDOW = 48;
	constexpr u32 HEX_DIGIT_BASE = 0x30;
	constexpr u32 HEX_LETTER_BASE = 0x37;
	constexpr u32 HEX_DIGIT_LIMIT = 0x0a;
	constexpr u32 FUNCTION_SEARCH_WINDOW = 1024;
	constexpr u32 BUFFER_SELECT_WINDOW = 16;
	constexpr u32 GETTER_CALL_WINDOW = 64;
	constexpr u32 CODE_ENGINE_SETUP_WINDOW = 16;
	constexpr u32 CODE_DISPATCH_PREAMBLE = 4;
	constexpr u32 CODE_ENTRY_WORDS = 2;
	constexpr u32 CODE_TYPE_SHIFT = 28;
	constexpr u32 CODE_INDEX_SHIFT = 3;
	constexpr u32 CODE_ADDRESS_MASK = 0x01ffffff;
	constexpr std::array<u32, 3> CODE_WRITE_SIZES = {1, 2, 4};
	constexpr u32 PCB_ID_TEXT_SIZE = 21;

	u32 Opcode(u32 word) { return word >> 26; }
	u32 Rs(u32 word) { return (word >> 21) & 0x1f; }
	u32 Rt(u32 word) { return (word >> 16) & 0x1f; }
	u32 Rd(u32 word) { return (word >> 11) & 0x1f; }
	u32 Shift(u32 word) { return (word >> 6) & 0x1f; }
	u32 Funct(u32 word) { return word & 0x3f; }
	u32 Immediate(u32 word) { return word & IMMEDIATE_MASK; }
	s32 SignedImmediate(u32 word) { return static_cast<s16>(Immediate(word)); }

	bool IsSpecial(u32 word, u32 funct) { return Opcode(word) == OPCODE_SPECIAL && Funct(word) == funct; }

	bool IsMove(u32 word, u32 destination, u32 source)
	{
		return IsSpecial(word, FUNCT_DADDU) && Rd(word) == destination && Rs(word) == source && Rt(word) == REGISTER_ZERO;
	}

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

	u32 WindowStart(u32 index, u32 window) { return index > window ? index - window : 0; }

	std::optional<u32> FindFunctionStart(std::span<const u32> ram, u32 index)
	{
		for (u32 candidate = index; candidate-- > WindowStart(index, FUNCTION_SEARCH_WINDOW);)
		{
			const u32 word = ram[candidate];
			if (Opcode(word) == OPCODE_ADDIU && Rs(word) == REGISTER_SP && Rt(word) == REGISTER_SP && SignedImmediate(word) < 0)
				return candidate;
		}
		return std::nullopt;
	}

	std::vector<u32> FindCallers(std::span<const u32> ram, u32 function)
	{
		const u32 call = (OPCODE_JAL << 26) | (function & JUMP_TARGET_MASK);
		std::vector<u32> out;
		for (u32 index = 0; index < ram.size(); index++)
		{
			if (ram[index] == call)
				out.push_back(index);
		}
		return out;
	}

	std::optional<u32> ReadGetterAddress(std::span<const u32> ram, u32 getter)
	{
		if (getter + 2 >= ram.size())
			return std::nullopt;

		const u32 high = ram[getter];
		const u32 jump = ram[getter + 1];
		const u32 low = ram[getter + 2];
		if (Opcode(high) != OPCODE_LUI || Rt(high) != REGISTER_V0 || !IsSpecial(jump, FUNCT_JR) || Rs(jump) != REGISTER_RA ||
			Opcode(low) != OPCODE_ADDIU || Rs(low) != REGISTER_V0 || Rt(low) != REGISTER_V0)
			return std::nullopt;

		return (Immediate(high) << 16) + SignedImmediate(low);
	}

	std::optional<u32> FindArgumentFromGetter(std::span<const u32> ram, u32 call)
	{
		if (call + 1 >= ram.size())
			return std::nullopt;

		const u32 delay = ram[call + 1];
		if (!IsSpecial(delay, FUNCT_DADDU) || Rd(delay) != REGISTER_A0 || Rt(delay) != REGISTER_ZERO)
			return std::nullopt;

		for (u32 index = call; index-- > std::max<u32>(WindowStart(call, GETTER_CALL_WINDOW), 2);)
		{
			if (IsMove(ram[index], Rs(delay), REGISTER_V0) && Opcode(ram[index - 2]) == OPCODE_JAL)
				return ReadGetterAddress(ram, ram[index - 2] & JUMP_TARGET_MASK);
		}
		return std::nullopt;
	}

	std::optional<u32> ReadHighLowPair(std::span<const u32> ram, u32 first, u32 low, u32 reg)
	{
		for (u32 high = low; high-- > first;)
		{
			if (Opcode(ram[high]) == OPCODE_LUI && Rt(ram[high]) == reg)
				return (Immediate(ram[high]) << 16) + SignedImmediate(ram[low]);
		}
		return std::nullopt;
	}

	struct CodeList
	{
		u32 entries;
		u32 count;
	};

	std::optional<CodeList> ReadCodeList(std::span<const u32> ram, u32 dispatch)
	{
		const u32 type = ram[dispatch];
		const u32 skip = ram[dispatch - 1];
		const u32 load = ram[dispatch - 2];
		const u32 entry = ram[dispatch - 3];
		const u32 scale = ram[dispatch - 4];
		if (!IsSpecial(type, FUNCT_SRL) || Shift(type) != CODE_TYPE_SHIFT || Rs(type) != REGISTER_ZERO)
			return std::nullopt;
		if (Opcode(skip) != OPCODE_BEQ || Rs(skip) != Rt(type) || Rt(skip) != REGISTER_ZERO)
			return std::nullopt;
		if (Opcode(load) != OPCODE_LW || Rt(load) != Rt(type) || Immediate(load) != 0)
			return std::nullopt;
		if (!IsSpecial(entry, FUNCT_ADDU) || Rd(entry) != Rs(load))
			return std::nullopt;
		if (!IsSpecial(scale, FUNCT_SLL) || Shift(scale) != CODE_INDEX_SHIFT || (Rd(scale) != Rs(entry) && Rd(scale) != Rt(entry)))
			return std::nullopt;

		const u32 list_register = Rd(scale) == Rs(entry) ? Rt(entry) : Rs(entry);
		const u32 setup_end = dispatch - CODE_DISPATCH_PREAMBLE;
		const u32 setup_start = WindowStart(setup_end, CODE_ENGINE_SETUP_WINDOW);
		std::optional<u32> entries;
		std::optional<u32> count_address;
		for (u32 index = setup_end; index-- > setup_start;)
		{
			const u32 word = ram[index];
			if (!entries.has_value() && Opcode(word) == OPCODE_ADDIU && Rs(word) == list_register && Rt(word) == list_register)
				entries = ReadHighLowPair(ram, setup_start, index, list_register);
			if (!count_address.has_value() && Opcode(word) == OPCODE_LW && Rs(word) == Rt(word))
				count_address = ReadHighLowPair(ram, setup_start, index, Rs(word));
		}
		if (!entries.has_value() || !count_address.has_value() || *count_address / sizeof(u32) >= ram.size())
			return std::nullopt;

		return CodeList{*entries / static_cast<u32>(sizeof(u32)), ram[*count_address / sizeof(u32)]};
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

std::vector<u32> Python2BootPatch::FindCalledPcbIdPrefixLoads(std::span<const u32> ram, u8 prefix)
{
	std::vector<u32> out;
	for (const u32 load : FindPcbIdPrefixLoads(ram, prefix))
	{
		const std::optional<u32> function = FindFunctionStart(ram, load);
		if (function.has_value() && !FindCallers(ram, *function).empty())
			out.push_back(load);
	}
	return out;
}

std::optional<u32> Python2BootPatch::FindPcbIdBufferAddress(std::span<const u32> ram, u32 prefix_load)
{
	const u32 first = WindowStart(prefix_load, BUFFER_SELECT_WINDOW);
	std::optional<u32> select;
	for (u32 index = prefix_load; index-- > first && !select.has_value();)
	{
		if (IsSpecial(ram[index], FUNCT_MOVN))
			select = index;
	}
	if (!select.has_value())
		return std::nullopt;

	const u32 white_buffer = Rs(ram[*select]);
	std::optional<u32> offset;
	u32 profile = REGISTER_ZERO;
	for (u32 index = *select; index-- > first && !offset.has_value();)
	{
		if (Opcode(ram[index]) == OPCODE_ADDIU && Rt(ram[index]) == white_buffer)
		{
			offset = SignedImmediate(ram[index]);
			profile = Rs(ram[index]);
		}
	}

	const std::optional<u32> function = FindFunctionStart(ram, prefix_load);
	if (!offset.has_value() || !function.has_value() ||
		std::none_of(ram.begin() + *function, ram.begin() + prefix_load, [&](u32 word) { return IsMove(word, profile, REGISTER_A0); }))
		return std::nullopt;

	for (const u32 call : FindCallers(ram, *function))
	{
		if (const std::optional<u32> profile_address = FindArgumentFromGetter(ram, call))
			return *profile_address + *offset;
	}
	return std::nullopt;
}

std::vector<u32> Python2BootPatch::FindCodeEngineWritesTo(std::span<const u32> ram, u32 address, u32 size)
{
	std::vector<u32> out;
	for (u32 dispatch = CODE_DISPATCH_PREAMBLE; dispatch < ram.size(); dispatch++)
	{
		const std::optional<CodeList> list = ReadCodeList(ram, dispatch);
		if (!list.has_value())
			continue;

		for (u32 entry = 0; entry < list->count; entry++)
		{
			const u32 index = list->entries + entry * CODE_ENTRY_WORDS;
			if (index >= ram.size())
				break;

			const u32 type = ram[index] >> CODE_TYPE_SHIFT;
			if (type >= CODE_WRITE_SIZES.size())
				continue;

			const u32 target = ram[index] & CODE_ADDRESS_MASK;
			if (target < address + size && address < target + CODE_WRITE_SIZES[type])
				out.push_back(index);
		}
	}
	return out;
}

bool Python2BootPatch::ApplyPcbId(u8 prefix)
{
	if (!eeMem)
		return false;

	const std::span<const u32> ram(reinterpret_cast<const u32*>(eeMem->Main), Ps2MemSize::MainRam / sizeof(u32));
	const std::vector<u32> loads = FindCalledPcbIdPrefixLoads(ram, prefix);
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

	const std::optional<u32> buffer = FindPcbIdBufferAddress(ram, loads.front());
	if (!buffer.has_value())
		return true;

	for (const u32 code : FindCodeEngineWritesTo(ram, *buffer, PCB_ID_TEXT_SIZE))
	{
		Console.WriteLn("P2IO: code %08X %08X at 0x%08x writes over the PCB ID at 0x%08x, disabled", ram[code], ram[code + 1],
			code * static_cast<u32>(sizeof(u32)), *buffer);
		memWrite32(code * sizeof(u32), 0);
	}
	return true;
}

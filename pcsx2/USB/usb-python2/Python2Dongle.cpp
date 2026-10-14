// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "USB/usb-python2/Python2Dongle.h"

#include "common/MD5Digest.h"
#include "common/StringUtil.h"

#include "fmt/format.h"

#include <algorithm>

namespace
{
	constexpr u32 SIGNATURE_OFFSET = 8;
	constexpr u32 MCODE_OFFSET = 14;
	constexpr u32 CRC_OFFSET = 39;
	constexpr u32 PACKED_LENGTH = 6;
	constexpr u32 TEXT_LENGTH = 8;
	constexpr u32 PCB_ID_BYTES = 10;
	constexpr u8 FIRST_TEXT_CHAR = 0x20;
	constexpr std::string_view WHITE_KEY = "E-AMUSE3";
	constexpr std::string_view WHITE_MCODE = "@@@@@@@@";
	constexpr std::array<u8, Python2Dongle::SERIAL_SIZE> PCB_ID_SERIAL_ORDER = {0, 6, 5, 4, 3, 2, 1, 7};
	constexpr std::array<u8, 16> SIGNATURE_SCRAMBLE = {
		0x0c, 0x02, 0x0f, 0x01, 0x07, 0x09, 0x04, 0x0a, 0x00, 0x0e, 0x03, 0x0d, 0x0b, 0x05, 0x08, 0x06};

	using Text = std::array<u8, TEXT_LENGTH>;
	using Packed = std::array<u8, PACKED_LENGTH>;

	Text ToText(std::string_view text)
	{
		Text out = {};
		std::copy_n(text.begin(), out.size(), out.begin());
		return out;
	}

	Packed Pack(const Text& text)
	{
		Text six = {};
		for (u32 i = 0; i < six.size(); i++)
			six[i] = static_cast<u8>((text[i] - FIRST_TEXT_CHAR) & 0x3f);

		Packed out = {};
		for (u32 group = 0; group < 2; group++)
		{
			const u8* in = &six[group * 4];
			u8* packed = &out[group * 3];
			packed[0] = static_cast<u8>(in[0] | (in[1] << 6));
			packed[1] = static_cast<u8>((in[1] >> 2) | (in[2] << 4));
			packed[2] = static_cast<u8>((in[2] >> 4) | (in[3] << 2));
		}
		return out;
	}

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

	Packed Sign(const Python2Dongle::Serial& serial, const Packed& packed_key)
	{
		std::array<u8, Python2Dongle::SERIAL_SIZE + PACKED_LENGTH> message = {};
		std::reverse_copy(serial.begin(), serial.end(), message.begin());
		std::copy(packed_key.begin(), packed_key.end(), message.begin() + serial.size());

		MD5Digest md5;
		md5.Update(message.data(), static_cast<u32>(message.size()));
		u8 digest[16];
		md5.Final(digest);

		std::array<u8, 18> scrambled = {};
		for (u32 i = 0; i < SIGNATURE_SCRAMBLE.size(); i++)
			scrambled[i] = digest[SIGNATURE_SCRAMBLE[i]];
		scrambled[16] = 0xde;
		scrambled[17] = 0xad;

		Packed out = {};
		for (u32 i = 0; i < out.size(); i++)
			out[i] = static_cast<u8>(scrambled[i] ^ scrambled[i + 6] ^ scrambled[i + 12]);
		return out;
	}
}

std::optional<Python2Dongle::PcbId> Python2Dongle::ParsePcbId(std::string_view text)
{
	if (text.size() != PCB_ID_BYTES * 2)
		return std::nullopt;

	std::array<u8, PCB_ID_BYTES> bytes = {};
	for (u32 i = 0; i < bytes.size(); i++)
	{
		std::string_view rest;
		const std::optional<u8> value = StringUtil::FromChars<u8>(text.substr(i * 2, 2), 16, &rest);
		if (!value.has_value() || !rest.empty())
			return std::nullopt;
		bytes[i] = value.value();
	}

	if (bytes[0] != PCB_ID_HEADER)
		return std::nullopt;

	PcbId out = {};
	out.prefix = bytes[1];
	for (u32 i = 0; i < SERIAL_SIZE; i++)
		out.serial[PCB_ID_SERIAL_ORDER[i]] = bytes[2 + i];
	return out;
}

Python2Dongle::Bytes Python2Dongle::BuildWhite(const Serial& serial)
{
	const Packed signature = Sign(serial, Pack(ToText(WHITE_KEY)));
	const Packed packed_mcode = Pack(ToText(WHITE_MCODE));

	Bytes out = {};
	std::copy(serial.begin(), serial.end(), out.begin());
	std::copy(signature.begin(), signature.end(), out.begin() + SIGNATURE_OFFSET);
	std::copy(packed_mcode.begin(), packed_mcode.end(), out.begin() + MCODE_OFFSET);
	out[CRC_OFFSET] = Crc8(&out[SIGNATURE_OFFSET], CRC_OFFSET - SIGNATURE_OFFSET);
	return out;
}

std::string Python2Dongle::FormatPcbId(u8 prefix, const Bytes& dongle)
{
	std::string out = fmt::format("{:02X}{:02X}", PCB_ID_HEADER, prefix);
	for (const u8 index : PCB_ID_SERIAL_ORDER)
		out += fmt::format("{:02X}", dongle[index]);
	return out;
}

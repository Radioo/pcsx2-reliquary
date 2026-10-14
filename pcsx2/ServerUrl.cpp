// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ServerUrl.h"

#include "common/StringUtil.h"

#include <cstring>
#include <mutex>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace
{
	std::string_view Authority(std::string_view url)
	{
		const size_t scheme_end = url.find("://");
		if (scheme_end != std::string_view::npos)
			url.remove_prefix(scheme_end + 3);

		const size_t path_start = url.find('/');
		if (path_start != std::string_view::npos)
			url = url.substr(0, path_start);

		return url;
	}
}

std::string ServerUrl::Host(std::string_view url)
{
	std::string_view host = Authority(url);
	const size_t port_start = host.rfind(':');
	if (port_start != std::string_view::npos)
		host = host.substr(0, port_start);

	return std::string(host);
}

u16 ServerUrl::Port(std::string_view url)
{
	const std::string_view authority = Authority(url);
	const size_t port_start = authority.rfind(':');
	if (port_start == std::string_view::npos)
		return 80;

	return StringUtil::FromChars<u16>(authority.substr(port_start + 1)).value_or(80);
}

std::optional<ServerUrl::Address> ServerUrl::Resolve(const std::string& host)
{
	if (host.empty())
		return std::nullopt;

#ifdef _WIN32
	static std::once_flag winsock_once;
	std::call_once(winsock_once, []() {
		WSADATA wsa_data;
		WSAStartup(MAKEWORD(2, 2), &wsa_data);
	});
#endif

	addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	addrinfo* results = nullptr;
	if (getaddrinfo(host.c_str(), nullptr, &hints, &results) != 0 || results == nullptr)
		return std::nullopt;

	Address address = {};
	const sockaddr_in* resolved = reinterpret_cast<const sockaddr_in*>(results->ai_addr);
	std::memcpy(address.data(), &resolved->sin_addr, address.size());
	freeaddrinfo(results);
	return address;
}

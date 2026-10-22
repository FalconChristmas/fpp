#pragma once

#include <curl/curl.h>

#include "SysSocket.h"

// libcurl opens its connections without close-on-exec, so every shell and tool
// fppd ran inherited the ones it had open, and held them past fppd's exit.
// Each easy handle opens them through OpenSocket() instead.
inline curl_socket_t CurlOpenSocket(void*, curlsocktype, struct curl_sockaddr* address) {
    return OpenSocket(address->family, address->socktype, address->protocol);
}

inline void SetCurlCloseOnExec(CURL* curl) {
    curl_easy_setopt(curl, CURLOPT_OPENSOCKETFUNCTION, static_cast<curl_opensocket_callback>(CurlOpenSocket));
}

/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the CC-BY-ND as described in the
 * included LICENSE.CC-BY-ND file.  This file may be modified for
 * personal use, but modified copies MAY NOT be redistributed in any form.
 */

#include "CapeUtils.h"
#include "fppversion_defines.h"

#include <array>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <fstream>
#include <pwd.h>
#include <unistd.h>

#include <dirent.h>
#include <libgen.h>
#include <string.h>

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#endif

#include <filesystem>

#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/decoder.h>
#endif

#if defined(PLATFORM_BBB) || defined(PLATFORM_BB64)
#define I2C_DEV 2
#elif defined(PLATFORM_PI)
#define I2C_DEV 1
#else
#define I2C_DEV 0
#endif

unsigned char fp_pub_pem[] = {
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x50,
    0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59, 0x2d, 0x2d, 0x2d,
    0x2d, 0x2d, 0x0a, 0x4d, 0x46, 0x59, 0x77, 0x45, 0x41, 0x59, 0x48, 0x4b,
    0x6f, 0x5a, 0x49, 0x7a, 0x6a, 0x30, 0x43, 0x41, 0x51, 0x59, 0x46, 0x4b,
    0x34, 0x45, 0x45, 0x41, 0x41, 0x6f, 0x44, 0x51, 0x67, 0x41, 0x45, 0x64,
    0x31, 0x79, 0x76, 0x42, 0x4a, 0x77, 0x70, 0x37, 0x5a, 0x62, 0x33, 0x51,
    0x33, 0x49, 0x33, 0x79, 0x58, 0x51, 0x78, 0x33, 0x61, 0x42, 0x51, 0x4a,
    0x6d, 0x6e, 0x37, 0x70, 0x77, 0x64, 0x57, 0x0a, 0x78, 0x37, 0x41, 0x41,
    0x2f, 0x62, 0x45, 0x32, 0x6a, 0x78, 0x50, 0x53, 0x7a, 0x6e, 0x78, 0x36,
    0x2f, 0x41, 0x58, 0x50, 0x61, 0x5a, 0x65, 0x75, 0x5a, 0x61, 0x37, 0x55,
    0x75, 0x54, 0x33, 0x78, 0x34, 0x39, 0x53, 0x42, 0x70, 0x5a, 0x2b, 0x5a,
    0x56, 0x4d, 0x45, 0x58, 0x63, 0x63, 0x54, 0x6f, 0x2f, 0x55, 0x7a, 0x44,
    0x62, 0x41, 0x3d, 0x3d, 0x0a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x45, 0x4e,
    0x44, 0x20, 0x50, 0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59,
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a
};
unsigned int fp_pub_pem_len = 174;

uint8_t dk_pub_pem[] = {
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x50,
    0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59, 0x2d, 0x2d, 0x2d,
    0x2d, 0x2d, 0x0a, 0x4d, 0x46, 0x59, 0x77, 0x45, 0x41, 0x59, 0x48, 0x4b,
    0x6f, 0x5a, 0x49, 0x7a, 0x6a, 0x30, 0x43, 0x41, 0x51, 0x59, 0x46, 0x4b,
    0x34, 0x45, 0x45, 0x41, 0x41, 0x6f, 0x44, 0x51, 0x67, 0x41, 0x45, 0x4e,
    0x32, 0x76, 0x35, 0x67, 0x69, 0x6c, 0x78, 0x38, 0x55, 0x70, 0x70, 0x4f,
    0x34, 0x6a, 0x63, 0x54, 0x38, 0x52, 0x53, 0x7a, 0x4c, 0x62, 0x49, 0x4d,
    0x78, 0x74, 0x76, 0x32, 0x73, 0x72, 0x30, 0x0a, 0x59, 0x73, 0x4f, 0x6c,
    0x56, 0x72, 0x68, 0x49, 0x5a, 0x5a, 0x4b, 0x62, 0x42, 0x6d, 0x39, 0x39,
    0x6f, 0x50, 0x4f, 0x71, 0x6a, 0x57, 0x4e, 0x55, 0x6f, 0x45, 0x30, 0x53,
    0x4c, 0x72, 0x61, 0x58, 0x49, 0x6d, 0x4b, 0x72, 0x75, 0x41, 0x74, 0x54,
    0x53, 0x52, 0x50, 0x31, 0x42, 0x42, 0x30, 0x42, 0x52, 0x6a, 0x38, 0x45,
    0x41, 0x77, 0x3d, 0x3d, 0x0a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x45, 0x4e,
    0x44, 0x20, 0x50, 0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59,
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a
};
unsigned int dk_pub_pem_len = 174;
uint8_t ww_pub_pem[] = {
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x50,
    0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59, 0x2d, 0x2d, 0x2d,
    0x2d, 0x2d, 0x0a, 0x4d, 0x46, 0x59, 0x77, 0x45, 0x41, 0x59, 0x48, 0x4b,
    0x6f, 0x5a, 0x49, 0x7a, 0x6a, 0x30, 0x43, 0x41, 0x51, 0x59, 0x46, 0x4b,
    0x34, 0x45, 0x45, 0x41, 0x41, 0x6f, 0x44, 0x51, 0x67, 0x41, 0x45, 0x65,
    0x75, 0x76, 0x6b, 0x42, 0x4b, 0x51, 0x32, 0x72, 0x72, 0x4c, 0x53, 0x6a,
    0x62, 0x5a, 0x68, 0x51, 0x77, 0x63, 0x62, 0x66, 0x70, 0x65, 0x4b, 0x5a,
    0x4c, 0x63, 0x52, 0x77, 0x79, 0x77, 0x4e, 0x0a, 0x6a, 0x2b, 0x38, 0x6f,
    0x44, 0x34, 0x62, 0x4f, 0x42, 0x64, 0x73, 0x4f, 0x6c, 0x63, 0x44, 0x59,
    0x47, 0x72, 0x47, 0x64, 0x76, 0x72, 0x36, 0x72, 0x69, 0x4b, 0x78, 0x7a,
    0x69, 0x46, 0x35, 0x66, 0x57, 0x57, 0x4f, 0x5a, 0x5a, 0x51, 0x4a, 0x74,
    0x50, 0x75, 0x46, 0x6d, 0x54, 0x51, 0x63, 0x70, 0x62, 0x73, 0x58, 0x34,
    0x45, 0x51, 0x3d, 0x3d, 0x0a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x45, 0x4e,
    0x44, 0x20, 0x50, 0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59,
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a
};
unsigned int ww_pub_pem_len = 174;
uint8_t jb_pub_pem[] = {
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x50,
    0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59, 0x2d, 0x2d, 0x2d,
    0x2d, 0x2d, 0x0a, 0x4d, 0x46, 0x59, 0x77, 0x45, 0x41, 0x59, 0x48, 0x4b,
    0x6f, 0x5a, 0x49, 0x7a, 0x6a, 0x30, 0x43, 0x41, 0x51, 0x59, 0x46, 0x4b,
    0x34, 0x45, 0x45, 0x41, 0x41, 0x6f, 0x44, 0x51, 0x67, 0x41, 0x45, 0x66,
    0x58, 0x33, 0x63, 0x55, 0x44, 0x65, 0x38, 0x64, 0x30, 0x73, 0x35, 0x5a,
    0x65, 0x4f, 0x65, 0x44, 0x56, 0x72, 0x77, 0x6b, 0x74, 0x59, 0x6f, 0x6d,
    0x77, 0x76, 0x35, 0x6e, 0x4e, 0x35, 0x49, 0x0a, 0x39, 0x53, 0x43, 0x43,
    0x65, 0x62, 0x6d, 0x48, 0x47, 0x6d, 0x50, 0x58, 0x4c, 0x43, 0x61, 0x54,
    0x78, 0x59, 0x79, 0x45, 0x66, 0x6d, 0x4e, 0x6a, 0x78, 0x67, 0x34, 0x56,
    0x44, 0x36, 0x6a, 0x78, 0x68, 0x65, 0x66, 0x30, 0x67, 0x71, 0x66, 0x7a,
    0x6d, 0x79, 0x75, 0x76, 0x77, 0x48, 0x57, 0x7a, 0x33, 0x72, 0x79, 0x4c,
    0x63, 0x51, 0x3d, 0x3d, 0x0a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x45, 0x4e,
    0x44, 0x20, 0x50, 0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59,
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a
};
unsigned int jb_pub_pem_len = 174;
uint8_t aah_pub_pem[] = {
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x50,
    0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59, 0x2d, 0x2d, 0x2d,
    0x2d, 0x2d, 0x0a, 0x4d, 0x46, 0x59, 0x77, 0x45, 0x41, 0x59, 0x48, 0x4b,
    0x6f, 0x5a, 0x49, 0x7a, 0x6a, 0x30, 0x43, 0x41, 0x51, 0x59, 0x46, 0x4b,
    0x34, 0x45, 0x45, 0x41, 0x41, 0x6f, 0x44, 0x51, 0x67, 0x41, 0x45, 0x54,
    0x6d, 0x6a, 0x33, 0x5a, 0x7a, 0x78, 0x4a, 0x32, 0x57, 0x53, 0x4f, 0x74,
    0x6d, 0x6d, 0x7a, 0x65, 0x72, 0x45, 0x36, 0x68, 0x4d, 0x35, 0x4a, 0x71,
    0x4c, 0x36, 0x56, 0x33, 0x79, 0x36, 0x4a, 0x0a, 0x4f, 0x49, 0x32, 0x51,
    0x58, 0x36, 0x41, 0x67, 0x6b, 0x42, 0x30, 0x6d, 0x4d, 0x54, 0x63, 0x42,
    0x47, 0x30, 0x52, 0x7a, 0x6a, 0x4d, 0x77, 0x63, 0x37, 0x39, 0x45, 0x41,
    0x65, 0x33, 0x44, 0x72, 0x7a, 0x35, 0x75, 0x5a, 0x45, 0x48, 0x30, 0x7a,
    0x57, 0x54, 0x78, 0x39, 0x6a, 0x33, 0x72, 0x5a, 0x7a, 0x49, 0x75, 0x58,
    0x47, 0x67, 0x3d, 0x3d, 0x0a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x45, 0x4e,
    0x44, 0x20, 0x50, 0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59,
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a
};
unsigned int aah_pub_pem_len = 174;

unsigned char fpp_pub_pem[] = {
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x50,
    0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59, 0x2d, 0x2d, 0x2d,
    0x2d, 0x2d, 0x0a, 0x4d, 0x46, 0x59, 0x77, 0x45, 0x41, 0x59, 0x48, 0x4b,
    0x6f, 0x5a, 0x49, 0x7a, 0x6a, 0x30, 0x43, 0x41, 0x51, 0x59, 0x46, 0x4b,
    0x34, 0x45, 0x45, 0x41, 0x41, 0x6f, 0x44, 0x51, 0x67, 0x41, 0x45, 0x59,
    0x73, 0x44, 0x6a, 0x63, 0x45, 0x41, 0x52, 0x38, 0x69, 0x4c, 0x6d, 0x39,
    0x53, 0x4a, 0x4b, 0x57, 0x6a, 0x42, 0x57, 0x79, 0x50, 0x56, 0x41, 0x72,
    0x76, 0x49, 0x41, 0x68, 0x79, 0x47, 0x71, 0x0a, 0x38, 0x69, 0x45, 0x53,
    0x76, 0x36, 0x4c, 0x37, 0x56, 0x52, 0x63, 0x58, 0x47, 0x4f, 0x49, 0x43,
    0x4e, 0x74, 0x2b, 0x56, 0x38, 0x51, 0x5a, 0x54, 0x55, 0x52, 0x4d, 0x6d,
    0x45, 0x58, 0x44, 0x64, 0x6e, 0x5a, 0x43, 0x68, 0x65, 0x45, 0x65, 0x6b,
    0x77, 0x76, 0x77, 0x47, 0x4b, 0x4b, 0x68, 0x4e, 0x36, 0x77, 0x5a, 0x71,
    0x64, 0x77, 0x3d, 0x3d, 0x0a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x45, 0x4e,
    0x44, 0x20, 0x50, 0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59,
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a
};
unsigned int fpp_pub_pem_len = 174;

unsigned char sh_pub_pem[] = {
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x50,
    0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59, 0x2d, 0x2d, 0x2d,
    0x2d, 0x2d, 0x0a, 0x4d, 0x46, 0x59, 0x77, 0x45, 0x41, 0x59, 0x48, 0x4b,
    0x6f, 0x5a, 0x49, 0x7a, 0x6a, 0x30, 0x43, 0x41, 0x51, 0x59, 0x46, 0x4b,
    0x34, 0x45, 0x45, 0x41, 0x41, 0x6f, 0x44, 0x51, 0x67, 0x41, 0x45, 0x71,
    0x30, 0x4f, 0x66, 0x6a, 0x51, 0x50, 0x39, 0x44, 0x73, 0x59, 0x45, 0x5a,
    0x43, 0x69, 0x76, 0x5a, 0x53, 0x73, 0x5a, 0x73, 0x55, 0x50, 0x47, 0x6b,
    0x66, 0x6b, 0x4c, 0x55, 0x72, 0x78, 0x61, 0x0a, 0x76, 0x51, 0x52, 0x79,
    0x34, 0x45, 0x4b, 0x46, 0x62, 0x6a, 0x59, 0x6a, 0x55, 0x72, 0x71, 0x39,
    0x53, 0x6e, 0x71, 0x6d, 0x51, 0x69, 0x50, 0x36, 0x58, 0x32, 0x6a, 0x67,
    0x30, 0x32, 0x35, 0x78, 0x46, 0x45, 0x51, 0x58, 0x61, 0x57, 0x49, 0x58,
    0x30, 0x4a, 0x77, 0x34, 0x74, 0x38, 0x43, 0x37, 0x36, 0x36, 0x64, 0x79,
    0x43, 0x51, 0x3d, 0x3d, 0x0a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x45, 0x4e,
    0x44, 0x20, 0x50, 0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59,
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a
};
unsigned int sh_pub_pem_len = 174;

static std::map<std::string, std::pair<const uint8_t*, int>> KEYS = {
    { "fp", std::pair(fp_pub_pem, fp_pub_pem_len) },
    { "dk", std::pair(dk_pub_pem, dk_pub_pem_len) },
    { "ww", std::pair(ww_pub_pem, ww_pub_pem_len) },
    { "jb", std::pair(jb_pub_pem, jb_pub_pem_len) },
    { "aah", std::pair(aah_pub_pem, aah_pub_pem_len) },
    { "fpp", std::pair(fpp_pub_pem, fpp_pub_pem_len) },
    { "sh", std::pair(sh_pub_pem, sh_pub_pem_len) }
};

template<typename... Args>
std::string string_sprintf(const char* format, Args... args) {
    int length = std::snprintf(nullptr, 0, format, args...);
    char* buf = new char[length + 1];
    std::snprintf(buf, length + 1, format, args...);

    std::string str(buf);
    delete[] buf;
    return std::move(str);
}

static std::string exec(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

static bool file_exists(const std::string& f) {
    return access(f.c_str(), F_OK) != -1;
}
static std::string trim(std::string str) {
    // remove trailing white space
    while (!str.empty() && (std::isspace(str.back()) || str.back() == 0))
        str.pop_back();

    // return residue after leading white space
    std::size_t pos = 0;
    while (pos < str.size() && std::isspace(str[pos]))
        ++pos;
    return str.substr(pos);
}
static std::string read_string(FILE* file, int len) {
    std::string buf;
    buf.reserve(len + 1);
    buf.resize(len);
    fread(&buf[0], 1, len, file);
    return trim(buf);
}
static void put_file_contents(const std::string& path, const uint8_t* data, int len) {
    FILE* f = fopen(path.c_str(), "w+b");
    fwrite(data, 1, len, f);
    fclose(f);

    struct passwd* pwd = getpwnam("fpp");
    chown(path.c_str(), pwd->pw_uid, pwd->pw_gid);

    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    chmod(path.c_str(), mode);
}
static uint8_t* get_file_contents(const std::string& path, int& len) {
    FILE* fp = fopen(path.c_str(), "rb");
    fseek(fp, 0L, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    uint8_t* data = (uint8_t*)malloc(len + 1);
    fread(data, 1, len, fp);
    fclose(fp);
    data[len] = 0;
    return data;
}

#ifdef PLATFORM_PI
static const std::string PLATFORM_DIR = "pi";
const std::string& getPlatformCapeDir() { return PLATFORM_DIR; }
#elif defined(PLATFORM_BB64)
static const std::string PLATFORM_DIR = "bb64";
const std::string& getPlatformCapeDir() { return PLATFORM_DIR; }
#elif defined(PLATFORM_BBB)
static const std::string PLATFORM_DIR_BBB = "bbb";
static const std::string PLATFORM_DIR_PB = "pb";
const std::string& getPlatformCapeDir() {
    int len = 0;
    char* buf = (char*)get_file_contents("/proc/device-tree/model", len);
    if (strcmp(&buf[10], "PocketBeagle") == 0) {
        free(buf);
        return PLATFORM_DIR_PB;
    }
    free(buf);
    return PLATFORM_DIR_BBB;
}

#else
static const std::string PLATFORM_DIR = "";
const std::string& getPlatformCapeDir() { return PLATFORM_DIR; }
#endif

static bool LoadJsonFromFile(const std::string& filename, Json::Value& root) {
    if (!file_exists(filename)) {
        return false;
    }
    int len = 0;
    char* data = (char*)get_file_contents(filename, len);

    Json::CharReaderBuilder builder;
    Json::CharReader* reader = builder.newCharReader();
    std::string errors;
    builder["collectComments"] = false;
    bool success = reader->parse(data, data + len, &root, &errors);
    delete reader;

    free(data);
    if (!success) {
        printf("Failed to parse %s: %s\n", filename.c_str(), errors.c_str());
    }
    return success;
}

static void disableOutputs(Json::Value& disables) {
    for (int x = 0; x < disables.size(); x++) {
        std::string file = disables[x]["file"].asString();
        std::string type = disables[x]["type"].asString();
        std::string subType = "";
        if (disables[x].isMember("subType")) {
            subType = disables[x]["subType"].asString();
        }
        std::string fullFile = "/home/fpp/media/" + file;
        if (file_exists(fullFile)) {
            Json::Value result;
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            std::string errors;
            std::ifstream istream(fullFile);
            std::stringstream buffer;
            buffer << istream.rdbuf();
            istream.close();

            std::string str = buffer.str();
            bool success = reader->parse(str.c_str(), str.c_str() + str.size(), &result, &errors);
            if (success) {
                bool changed = false;
                if (result.isMember("channelOutputs")) {
                    for (int co = 0; co < result["channelOutputs"].size(); co++) {
                        if (result["channelOutputs"][co]["type"].asString() == type && (subType == "" || (result["channelOutputs"][co].isMember("subType") && result["channelOutputs"][co]["subType"].asString() == subType))) {
                            if (result["channelOutputs"][co]["enabled"].asInt() == 1) {
                                result["channelOutputs"][co]["enabled"] = 0;
                                changed = true;
                            }
                        }
                    }
                }
                if (changed) {
                    Json::StreamWriterBuilder wbuilder;
                    std::string resultStr = Json::writeString(wbuilder, result);
                    put_file_contents(fullFile, (const uint8_t*)resultStr.c_str(), resultStr.size());
                }
            }
        }
    }
}
static bool processBootConfig(Json::Value& bootConfig) {
#if defined(PLATFORM_PI)
    const std::string fileName = FPP_BOOT_DIR "/config.txt";
#elif defined(PLATFORM_BBB)
    const std::string fileName = FPP_BOOT_DIR "/uEnv.txt";
#elif defined(PLATFORM_BB64)
    // TODO - booting is VERY different on BB64
    const std::string fileName;
#elif defined(PLATFORM_ARMBIAN)
    const std::string fileName = FPP_BOOT_DIR "/armbianEnv.txt";
#else
    // unknown platform
    const std::string fileName;
#endif

    if (fileName.empty())
        return false;

    int len = 0;
    uint8_t* data = get_file_contents(fileName, len);
    if (len == 0) {
        return false;
    }
    std::string current = (char*)data;
    std::string orig = current;
    if (bootConfig.isMember("remove")) {
        for (int x = 0; x < bootConfig["remove"].size(); x++) {
            std::string v = bootConfig["remove"][x].asString();
            size_t pos = std::string::npos;
            while ((pos = current.find(v)) != std::string::npos) {
                // If found then erase it from string
                current.erase(pos, v.length());
            }
        }
    }
    if (bootConfig.isMember("append")) {
        for (int x = 0; x < bootConfig["append"].size(); x++) {
            std::string v = bootConfig["append"][x].asString();
            size_t pos = current.find(v);
            if (pos == std::string::npos) {
                // If not  found then append it
                printf("Adding config option: %s\n", v.c_str());
                current += "\n";
                current += v;
                current += "\n";
            }
        }
    }
    free(data);
    return current != orig;
}
static void handleReboot(bool r) {
    if (r) {
        if (file_exists("/.fppcapereboot")) {
            const std::string msg = "Reboot Cycle Detected with FPP Cape Detect";
            put_file_contents("/home/fpp/media/tmp/reboot", (const uint8_t*)msg.c_str(), msg.length());
            printf("%s\n", msg.c_str());
        } else {
            const uint8_t data[2] = { 32, 0 };
            put_file_contents("/.fppcapereboot", data, 1);
            sync();
            setuid(0);
            reboot(RB_AUTOBOOT);
            exit(0);
        }
    } else {
        remove("/.fppcapereboot");
    }
}
static void copyFile(const std::string& src, const std::string& target) {
    int s, t;
    s = open(src.c_str(), O_RDONLY);
    if (s == -1) {
        printf("Failed to open src %s - %s\n", src.c_str(), strerror(errno));
        return;
    }

    size_t fsep = target.find_last_of("/");
    if (fsep != std::string::npos)
        mkdir(target.substr(0, fsep).c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH); // ensure target folder exists
                                                                                                //         if (mkdir(path, mode) != 0 && errno != EEXIST)
                                                                                                //     else if (!S_ISDIR(st.st_mode)) errno = ENOTDIR;

    t = open(target.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (t == -1) {
        printf("Failed to open target %s - %s\n", target.c_str(), strerror(errno));
        close(s);
        return;
    }
    uint8_t buf[257];
    while (true) {
        int l = read(s, buf, 256);
        if (l == -1) {
            printf("Error copying file %s - %s\n", src.c_str(), strerror(errno));
            close(s);
            close(t);
            return;
        } else if (l == 0) {
            close(s);
            close(t);
            return;
        }
        write(t, buf, l);
    }
}
// similar to copy file, but do in 512 increments and check if the last 64 bytes are 0xFF
// to see if we can detect the end of the actual data and stop at that point as reading
// more data then needed is slow on the i2c bus
static void copyEEPROM(const std::string& src, const std::string& target) {
    int s, t;
    s = open(src.c_str(), O_RDONLY);
    if (s == -1) {
        printf("Failed to open src %s - %s\n", src.c_str(), strerror(errno));
        return;
    }

    size_t fsep = target.find_last_of("/");
    if (fsep != std::string::npos)
        mkdir(target.substr(0, fsep).c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH); // ensure target folder exists
                                                                                                //         if (mkdir(path, mode) != 0 && errno != EEXIST)
                                                                                                //     else if (!S_ISDIR(st.st_mode)) errno = ENOTDIR;

    t = open(target.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (t == -1) {
        printf("Failed to open target %s - %s\n", target.c_str(), strerror(errno));
        close(s);
        return;
    }
    constexpr int BUFSIZE = 256;
    uint8_t buf[BUFSIZE + 1];
    while (true) {
        int l = read(s, buf, BUFSIZE);
        while (l > 0 && l < BUFSIZE) {
            int r = read(s, &buf[l], BUFSIZE - l);
            if (r == -1) {
                printf("Error copying file %s - %s\n", src.c_str(), strerror(errno));
                close(s);
                close(t);
                return;
            }
            l += r;
        }
        if (l == -1) {
            printf("Error copying file %s - %s\n", src.c_str(), strerror(errno));
            close(s);
            close(t);
            return;
        } else if (l == 0) {
            close(s);
            close(t);
            return;
        }
        write(t, buf, l);
        if (l > 64) {
            bool allFF = true;
            for (int x = 0; x < 64; x++) {
                if (buf[l - x - 1] != 0xFF) {
                    allFF = false;
                    break;
                }
            }
            if (allFF) {
                close(s);
                close(t);
                return;
            }
        }
    }
}
static size_t fileSize(const std::string& target) {
    struct stat stat_buf;
    int rc = stat(target.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : 0;
}

static void copyIfNotExist(const std::string& src, const std::string& target) {
    if (fileSize(target) > 0) {
        return;
    }
    copyFile(src, target);
}
static void removeIfExist(const std::string& src) {
    if (file_exists(src)) {
        unlink(src.c_str());
    }
}
void setOwnerGroup(const std::string& filename) {
    static struct passwd* pwd = getpwnam("fpp");
    if (pwd) {
        chown(filename.c_str(), pwd->pw_uid, pwd->pw_gid);
    }
}
bool setFilePerms(const std::string& filename) {
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    chmod(filename.c_str(), mode);
    setOwnerGroup(filename);
    return true;
}

static bool handleCapeOverlay(const std::string& outputPath) {
#ifdef PLATFORM_BB64
    static const std::string src = outputPath + "/tmp/fpp-cape-overlay-bb64.dtb";
    static const std::string target = "/boot/firmware/overlays/fpp-cape-overlay.dtb";
    static const std::string overlay = "/proc/device-tree/chosen/overlays/fpp-cape-overlay";
    if (file_exists(src)) {
        int slen = 0;
        int tlen = 0;
        uint8_t* sd = get_file_contents(src, slen);
        uint8_t* td = get_file_contents(target, tlen);
        if (slen != tlen || memcmp(sd, td, slen) != 0) {
            copyFile(src, target);
            free(sd);
            free(td);
            return true;
        }
        free(sd);
        free(td);
    } else if (file_exists(overlay)) {
        int len = 0;
        char* c = (char*)get_file_contents(overlay, len);
        if (strcmp(c, "DEFAULT_CAPE_OVERLAY") != 0) {
            // not the default cape overlay, need to flip back to default
            std::string r = exec("make -f /opt/fpp/capes/drivers/bb64/Makefile install_cape_overlay");
            printf("%s\n", r.c_str());
            free(c);
            return true;
        }
        free(c);
    }
#endif
    return false;
}

#ifdef PLATFORM_BBB
bool isPocketBeagle() {
    bool ret = false;
    FILE* file = fopen("/proc/device-tree/model", "r");
    if (file) {
        char buf[256];
        fgets(buf, 256, file);
        fclose(file);
        if (strcmp(&buf[10], "PocketBeagle") == 0) {
            ret = true;
        }
    }
    return ret;
}
void ConfigurePin(const char* pin, const char* mode) {
    char dir_name[255];
    snprintf(dir_name, sizeof(dir_name),
             "/sys/devices/platform/ocp/ocp:%s_pinmux/state",
             pin);
    FILE* dir = fopen(dir_name, "w");
    if (!dir) {
        return;
    }
    fprintf(dir, "%s\n", mode);
    fclose(dir);
}
#endif
void ConfigureI2C1BusPins(bool enable) {
#ifdef PLATFORM_BBB
    if (isPocketBeagle()) {
        ConfigurePin("P2_09", enable ? "i2c" : "default");
        ConfigurePin("P2_11", enable ? "i2c" : "default");
    } else {
        ConfigurePin("P9_17", enable ? "i2c" : "default");
        ConfigurePin("P9_18", enable ? "i2c" : "default");
    }
#endif
}

static std::map<std::string, std::string> CONFIG_EEPROM_UPGRADE_MAP = {
    { "RPIWS281X:PiHat", "PiHat" },
    { "spixels:spixels", "spixels" },
    { "RPIWS281X:rPi-MFC", "rPi-MFC" },
    { "RPIWS281X:rPi-28D", "rPi-28D" },
    { "BBB48String:F8-B", "F8-B" },
    { "BBB48String:F8-B-16", "F8-B" },
    { "BBB48String:F8-B-20", "F8-B" },
    { "BBB48String:F8-B-EXP", "F8-B" },
    { "BBB48String:F8-B-EXP-32", "F8-B" },
    { "BBB48String:F8-B-EXP-36", "F8-B" },
    { "BBB48String:F16-B", "F16-B" },
    { "BBB48String:F16-B-32", "F16-B" },
    { "BBB48String:F16-B-48", "F16-B" },
    { "BBB48String:F4-B", "F4-B" },
    { "BBB48String:F32-B", "F-32" },
    { "BBB48String:F32-B-44", "F-32" },
    { "BBB48String:F32-B-48", "F-32" },
    { "BBB48String:RGBCape24", "RGB-123" },
    { "BBB48String:RGBCape48A", "RGB-123" },
    { "BBB48String:RGBCape48C", "RGB-123" },
    { "BBB48String:RGBCape48F", "RGB-123" },
    { "BBB48String:PB16", "PB16" },
    { "BBB48String:PB16-EXP", "PB16" }
};

class CapeInfo {
public:
    CapeInfo(bool ro) :
        readOnly(ro),
        bus(I2C_DEV) {
        char buf[256] = "/tmp/fppcuXXXXXX";
        outputPath = mkdtemp(buf);

        std::string tmpPath = outputPath + "/tmp";
        mkdir(tmpPath.c_str(), 0700);

        findCapeEEPROM();
        if (EEPROM != "") {
            try {
                parseEEPROM();
                processEEPROM();
            } catch (std::exception& ex) {
                corruptEEPROM = true;
                printf("Corrupt EEPROM: %s\n", ex.what());
            }
        }
        loadFiles();
        std::filesystem::remove_all(outputPath);
    }

    ~CapeInfo() {
    }

    CapeUtils::CapeStatus capeStatus() {
        if (corruptEEPROM) {
            return CapeUtils::CapeStatus::CORRUPT;
        }
        if (validSignature) {
            if (ORIGEEPROM.find("sys/bus/i2c") != std::string::npos) {
                return CapeUtils::CapeStatus::SIGNED;
            } else if (devsn != "") {
                return CapeUtils::CapeStatus::SIGNED;
            }
            return CapeUtils::CapeStatus::SIGNED_GENERIC;
        }
        if (!validSignature && EEPROM != "") {
            return CapeUtils::CapeStatus::UNSIGNED;
        }
        return CapeUtils::CapeStatus::NOT_PRESENT;
    }

    bool hasFile(const std::string& path) {
        return fileMap.find(path) != fileMap.end();
    }
    const std::vector<uint8_t>& getFile(const std::string& path) {
        static std::vector<uint8_t> EMPTY;
        if (hasFile(path)) {
            return fileMap[path];
        }
        return EMPTY;
    }

    int getLicensedOutputs() {
        if (hasSignature && validSignature && (fkeyId != "")) {
            if (fkeyId != "fp") {
                return 9999;
            } else if (signedExtras.find("licensePorts") != signedExtras.end()) {
                return atoi(signedExtras["licensePorts"][0].c_str());
            }
        }
        return 0;
    }
    const std::string& getKeyId() const {
        return fkeyId;
    }
    const Json::Value& getCapeInfo() const {
        return capeInfo;
    }

private:
    void loadFiles() {
        std::vector<std::string> files;
        if (!readOnly) {
            getFileList(outputPath + "/tmp", "", files);
            for (auto a : files) {
                std::string src = outputPath + "/tmp" + a;
                std::string target = "/home/fpp/media/tmp" + a;

                if (target[target.length() - 1] == '/') {
                    mkdir(target.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
                    setOwnerGroup(target);
                } else {
                    copyFile(src, target);
                    setFilePerms(target);
                }
            }
            files.clear();
        }
        getFileList(outputPath, "", files);
        for (auto a : files) {
            std::string src = outputPath + a;

            if (a[a.length() - 1] != '/') {
                int len = fileSize(src);
                fileMap[a].resize(len);
                FILE* f = fopen(src.c_str(), "rb");
                int l2 = fread(&fileMap[a][0], 1, len, f);
                fclose(f);
            }
        }
    }

    void findCapeEEPROM() {
        waitForI2CBus(bus);
        if (bus == 2 && !HasI2CDevice(0x50, bus)) {
            printf("Did not find 0x50 on i2c2, trying i2c1.\n");
            ConfigureI2C1BusPins(true);
            if (HasI2CDevice(0x50, 1)) {
                bus = 1;
            } else {
                ConfigureI2C1BusPins(false);
            }
        }
        if (HasI2CDevice(0x50, bus)) {
            EEPROM = string_sprintf("/sys/bus/i2c/devices/%d-0050/eeprom", bus);
            if (!file_exists(EEPROM)) {
                std::string newDevFile = string_sprintf("/sys/bus/i2c/devices/i2c-%d/new_device", bus);
                int f = open(newDevFile.c_str(), O_WRONLY);
                write(f, "24c256 0x50", 11);
                close(f);

                for (int x = 0; x < 1500; x++) {
                    if (file_exists(EEPROM)) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                if (!file_exists(EEPROM)) {
                    // try again
                    newDevFile = string_sprintf("/sys/bus/i2c/devices/i2c-%d/new_device", bus);
                    f = open(newDevFile.c_str(), O_WRONLY);
                    write(f, "24c256 0x50", 11);
                    close(f);
                    for (int x = 0; x < 200; x++) {
                        if (file_exists(EEPROM)) {
                            break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
            }
            if (!file_exists(EEPROM)) {
                printf("Couldn't get eeprom\n");
            }
            printf("Copying eeprom %s -> %s\n", EEPROM.c_str(), "/home/fpp/media/tmp/eeprom.bin");
            removeIfExist("/home/fpp/media/tmp/eeprom.bin");
            copyEEPROM(EEPROM, "/home/fpp/media/tmp/eeprom.bin");
            struct passwd* pwd = getpwnam("fpp");
            if (pwd) {
                chown("/home/fpp/media/tmp/eeprom.bin", pwd->pw_uid, pwd->pw_gid);
            }
            ORIGEEPROM = EEPROM;
            put_file_contents("/home/fpp/media/tmp/eeprom_location.txt", (uint8_t*)ORIGEEPROM.c_str(), ORIGEEPROM.size());
            EEPROM = "/home/fpp/media/tmp/eeprom.bin";

            std::string newDevFile = string_sprintf("/sys/bus/i2c/devices/i2c-%d/delete_device", bus);
            int f = open(newDevFile.c_str(), O_WRONLY);
            write(f, "0x50", 5);
            close(f);
        } else {
            printf("Did not find eeprom on i2c.\n");
        }
        if (!file_exists(EEPROM)) {
            printf("EEPROM file doesn't exist %s.\n", EEPROM.c_str());
            EEPROM = "/opt/fpp-vendor/cape-eeprom.bin";
        }
        if (!file_exists(EEPROM)) {
            EEPROM = "/home/fpp/media/config/cape-eeprom.bin";
        }
        if (!file_exists(EEPROM)) {
            EEPROM = mapV5Config(EEPROM);
        }
        if (!file_exists(EEPROM)) {
            EEPROM = checkUnsupported(EEPROM, bus);
        }
        if (!file_exists(EEPROM)) {
            printf("Could not detect any cape\n");
            EEPROM = "";
            handleReboot(handleCapeOverlay("/home/fpp/media/"));
            return;
        }
        printf("Using %s\n", EEPROM.c_str());
        uint8_t buffer[12];
        FILE* file = fopen(EEPROM.c_str(), "rb");
        int l = fread(buffer, 1, 6, file);
        if (buffer[0] != 'F' || buffer[1] != 'P' || buffer[2] != 'P' || buffer[3] != '0' || buffer[4] != '2') {
            printf("EEPROM header miss-match\n");
            EEPROM = "/home/fpp/media/config/cape-eeprom.bin";
            if (!file_exists(EEPROM)) {
                EEPROM = "";
            } else {
                printf("Using %s\n", EEPROM.c_str());
            }
        }
        fclose(file);
    }

    void parseEEPROM() {
        uint8_t* buffer = new uint8_t[32768]; // 32K is the largest eeprom we support, more than enough
        FILE* file = fopen(EEPROM.c_str(), "rb");
        if (file == nullptr) {
            printf("Failed to open eeprom: %s\n", EEPROM.c_str());
            printf("   error: %s\n", strerror(errno));
            return;
        }
        int l = fread(buffer, 1, 6, file);

        if (buffer[0] == 'F' && buffer[1] == 'P' && buffer[2] == 'P' && buffer[3] == '0' && buffer[4] == '2') {
            cape = read_string(file, 26);   // cape name + nulls
            capev = read_string(file, 10);  // cape version + nulls
            capesn = read_string(file, 16); // cape serial# + nulls
            printf("Found cape %s, Version %s, Serial Number: %s\n", cape.c_str(), capev.c_str(), capesn.c_str());

            std::string flenStr = read_string(file, 6); // length of the section
            int flen = std::stoi(flenStr);
            while (flen) {
                int flag = std::stoi(read_string(file, 2));
                std::string path;
                if (flag < 50) {
                    path = outputPath + "/";
                    path += read_string(file, 64);
                }
                switch (flag) {
                case 0:
                case 1:
                case 2:
                case 3: {
                    int l = fread(buffer, 1, flen, file);
                    put_file_contents(path, buffer, flen);
                    char* s1 = strdup(path.c_str());
                    std::string dir = dirname(s1);
                    free(s1);
                    if (flag == 1) {
                        std::string cmd = "cd " + dir + "; unzip " + path + " 2>&1";
                        exec(cmd);
                        unlink(path.c_str());
                    } else if (flag == 2) {
                        std::string cmd = "cd " + dir + "; tar -xzf " + path + " 2>&1";
                        exec(cmd);
                        unlink(path.c_str());
                    } else if (flag == 3) {
                        std::string cmd = "cd " + dir + "; tar -xjf " + path + " 2>&1";
                        exec(cmd);
                        unlink(path.c_str());
                    }
                    printf("- extracted file: %s\n", path.c_str());
                    break;
                }
                case 96: {
                    if (hasSignature && validEpromLocation && validSignature) {
                        capesn = read_string(file, 16);
                        devsn = read_string(file, 42);
                    } else {
                        // skip
                        read_string(file, 16);
                        read_string(file, 42);
                    }
                    break;
                }
                case 97: {
                    std::string eKey = read_string(file, 12);
                    std::string eValue = read_string(file, flen - 12);
                    if (hasSignature && validEpromLocation && validSignature) {
                        signedExtras[eKey].push_back(eValue);
                    } else {
                        extras[eKey].push_back(eValue);
                    }
                    break;
                }
                case 98: {
                    std::string type = read_string(file, 2);
                    if (type == "1") {
                        if (ORIGEEPROM.find("sys/bus/i2c") == std::string::npos) {
                            validEpromLocation = false;
                        }
                    } else if (type == "2") {
                        if (EEPROM.find("cape-eeprom.bin") == std::string::npos) {
                            validEpromLocation = false;
                        }
                    } else if (type != "0") {
                        validEpromLocation = false;
                    }

                    if (validEpromLocation) {
                        printf("- eeprom location is valid\n");
                    } else {
                        printf("- ERROR eeprom location is NOT valid\n");
                    }
                    break;
                }
                case 99: {
                    hasSignature = true;
                    fkeyId = read_string(file, 6);
                    fread(buffer, 1, flen - 6, file);
                    DoSignatureVerify(file, fkeyId, path, validSignature, buffer, flen - 6);

                    if (validSignature)
                        printf("- signature verified for key ID: %s\n", fkeyId.c_str());
                    else
                        printf("- ERROR signature is NOT valid for key ID: %s\n", fkeyId.c_str());
                    break;
                }
                default:
                    fseek(file, flen, SEEK_CUR);
                    printf("Don't know how to handle -%s- with type %d and length %d\n", path.c_str(), flag, flen);
                }
                flenStr = read_string(file, 6); // length of the section
                flen = std::stoi(flenStr);
            }
        }
        fclose(file);
        delete[] buffer;

        if (signedExtras.find("deviceSerial") != signedExtras.end()) {
            char serialCmd[] = "sed -n 's/^Serial.*: //p' /proc/cpuinfo";
            std::string serialNumber = trim(exec(serialCmd));
            if (serialNumber != signedExtras["deviceSerial"][0]) {
                // If the device Serial Number does not match the signed Serial Number in the EEPROM, treat like an invalid signature
                validSignature = false;
                hasSignature = true;
                printf("- ERROR device serial number in EEPROM (%s) does NOT match physical device (%s)\n",
                       signedExtras["deviceSerial"][0].c_str(), serialNumber.c_str());
            }
        }

        if (!validEpromLocation) {
            // if the eeprom location is not valid, treat like an invalid signature
            validSignature = false;
            hasSignature = true;
        }
        if (!hasSignature || !validSignature) {
            // not a valid signed eeprom, remove stuff that requires it
            removeIfExist(outputPath + "/tmp/cape-sensors.json");
            removeIfExist(outputPath + "/tmp/cape-inputs.json");
            removeIfExist(outputPath + "/tmp/defaults/config/sensors.json");
        }
    }

    void processEEPROM() {
        // if the cape-info has default settings and those settings are not already set, set them
        // also put the serialNumber into the cape-info for display
        if (file_exists(outputPath + "/tmp/cape-info.json")) {
            Json::Value result;
            if (LoadJsonFromFile(outputPath + "/tmp/cape-info.json", result)) {
                std::set<std::string> removes;
                if (hasSignature) {
                    result["validEepromLocation"] = validEpromLocation;
                }
                if (ORIGEEPROM != "") {
                    result["eepromLocation"] = ORIGEEPROM;
                } else {
                    result["eepromLocation"] = EEPROM;
                }
                if (result.isMember("removeSettings")) {
                    for (int x = 0; x < result["removeSettings"].size(); x++) {
                        std::string v = result["removeSettings"][x].asString();
                        removes.insert(v);
                    }
                }
                for (auto kv : extras) {
                    if (kv.second.size() > 1) {
                        for (auto e : kv.second) {
                            result[kv.first].append(e);
                        }
                    } else {
                        result[kv.first] = kv.second[0];
                    }
                }
                if (result.isMember("signed")) {
                    // make sure any "not really signed" extras are removed from the cape-info
                    result.removeMember("signed");
                }
                for (auto kv : signedExtras) {
                    if (kv.second.size() > 1) {
                        for (auto e : kv.second) {
                            result["signed"][kv.first].append(e);
                        }
                    } else {
                        result["signed"][kv.first] = kv.second[0];
                    }
                }

                if (EEPROM.find("sys/bus/i2c") == std::string::npos && devsn == "" && validSignature) {
                    removes.insert("FetchVendorLogos");
                }
                if (result["id"].asString() != "Unsupported" && result["id"].asString() != "Unknown") {
                    result["serialNumber"] = capesn;
                }
                std::vector<std::string> lines;
                bool settingsChanged = false;
                if (!validSignature) {
                    if (result.isMember("verifiedKeyId"))
                        result.removeMember("verifiedKeyId");
                } else {
                    result["verifiedKeyId"] = fkeyId;
                }
                if (!readOnly) {
                    readSettingsFile(lines);
                    for (auto& v : removes) {
                        std::string found = "";
                        for (int l = 0; l < lines.size(); l++) {
                            if (lines[l].find(v) == 0) {
                                lines.erase(lines.begin() + l);
                                settingsChanged = true;
                                break;
                            }
                        }
                    }
                    if (result.isMember("defaultSettings")) {
                        std::map<std::string, std::string> defaults;
                        for (auto a : result["defaultSettings"].getMemberNames()) {
                            std::string v = result["defaultSettings"][a].asString();
                            bool found = false;
                            for (auto l : lines) {
                                if (l.find(a) == 0) {
                                    found = true;
                                }
                            }
                            if (!found) {
                                lines.push_back(a + " = \"" + v + "\"");
                                settingsChanged = true;
                            }
                        }
                    } else if (!hasSignature || !validSignature) {
                        std::string v = "statsPublish";
                        bool found = false;
                        for (int l = 0; l < lines.size(); l++) {
                            if (lines[l].find(v) == 0) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            lines.push_back(v + " = \"Enabled\"");
                            settingsChanged = true;
                        }
                        if (result.isMember("sendStats")) {
                            result.removeMember("sendStats");
                        }
                    }

                    bool reboot = false;
                    if (result.isMember("bootConfig")) {
                        // if the cape requires changes/update to config.txt (Pi) or uEnv.txt (BBB)
                        // we need to process them and see if we have to apply the changes and reboot or not
                        processBootConfig(result["bootConfig"]);
                    }
                    if (result.isMember("copyFiles")) {
                        // if the cape requires certain files copied into place (asoundrc for example)
                        for (auto src : result["copyFiles"].getMemberNames()) {
                            std::string target = result["copyFiles"][src].asString();

                            if (validSignature && (src[0] != '/')) {
                                src = outputPath + "/" + src;
                            }
                            if (target[0] != '/') {
                                target = "/home/fpp/media/" + target;
                            }
                            copyFile(src, target);
                            setFilePerms(target);
                        }
                    }
                    reboot = handleCapeOverlay(outputPath);
                    handleReboot(reboot);
                    if (result.isMember("modules")) {
                        // if the cape requires kernel modules, load them at this
                        // time so they will be available later
                        for (int x = 0; x < result["modules"].size(); x++) {
                            std::string v = "/sbin/modprobe " + result["modules"][x].asString() + " 2> /dev/null  > /dev/null";
                            exec(v.c_str());
                        }
                    }

                    if (result.isMember("i2cDevices") && !readOnly) {
                        // if the cape has i2c devices on it that need to be registered, load them at this
                        // time so they will be available later
                        for (int x = 0; x < result["i2cDevices"].size(); x++) {
                            std::string v = result["i2cDevices"][x].asString();

                            std::string newDevFile = string_sprintf("/sys/bus/i2c/devices/i2c-%d/new_device", bus);
                            int f = open(newDevFile.c_str(), O_WRONLY);
                            std::string newv = string_sprintf("%s", v.c_str());
                            write(f, newv.c_str(), newv.size());
                            close(f);
                        }
                    }
                    if (validSignature && result.isMember("disableOutputs")) {
                        disableOutputs(result["disableOutputs"]);
                    }
                    if (settingsChanged) {
                        lines.push_back("BootActions = \"settings\"");
                        writeSettingsFile(lines);
                    }
                }
                Json::StreamWriterBuilder wbuilder;
                std::string resultStr = Json::writeString(wbuilder, result);
                put_file_contents(outputPath + "/tmp/cape-info.json", (const uint8_t*)resultStr.c_str(), resultStr.size());
                setFilePerms(outputPath + "/tmp/cape-info.json");
            }
            capeInfo = result;
        } else {
            printf("Did not find cape-info.json\n");
        }

        // if there are default configurations, copy them into place if they dont already exist
        if (!readOnly && (!hasSignature || validSignature)) {
            std::vector<std::string> files;
            getFileList(outputPath + "/tmp/defaults", "", files);
            for (auto a : files) {
                std::string src = outputPath + "/tmp/defaults" + a;
                std::string target = "/home/fpp/media" + a;

                if (target[target.length() - 1] == '/') {
                    mkdir(target.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
                } else {
                    copyIfNotExist(src, target);
                    setOwnerGroup(target);
                }
            }
        }
        std::vector<std::string> files;
        getFileList(outputPath + "/tmp/strings", "", files);
        for (auto a : files) {
            std::string src = outputPath + "/tmp/strings" + a;
            Json::Value result;
            Json::CharReaderBuilder factory;
            std::string errors;
            std::ifstream istream(src);
            bool success = Json::parseFromStream(factory, istream, &result, &errors);
            if (success) {
                result["supportsSmartReceivers"] = validSignature;
                Json::StreamWriterBuilder wbuilder;
                std::string resultStr = Json::writeString(wbuilder, result);
                put_file_contents(src, (const uint8_t*)resultStr.c_str(), resultStr.size());
            }
        }
    }

    bool waitForI2CBus(int i2cBus) {
        char buf[256];
        snprintf(buf, sizeof(buf), "/sys/bus/i2c/devices/i2c-%d/new_device", i2cBus);

        bool has0 = access("/sys/bus/i2c/devices/i2c-0/new_device", F_OK) != -1;

        // wait for up to 15 seconds for the i2c bus to appear
        // if it's already there, this should be nearly immediate
        for (int x = 0; x < 1500; x++) {
            if (access(buf, F_OK) != -1) {
                return true;
            }

            // after 1/10 of a second, if there is a 0 bus, then it's likely
            // that the wanted bus won't exist
            if (x > 100 && has0) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }

    bool HasI2CDevice(int i, int i2cBus) {
        char buf[256];
        snprintf(buf, sizeof(buf), "i2cdetect -y -r %d 0x%X 0x%X", i2cBus, i, i);
        std::string result = exec(buf);
        return result != "" && result.find("--") == std::string::npos;
    }
    std::string checkUnsupported(const std::string& orig, int i2cbus) {
        return orig;
    }

    std::string mapV5Config(const std::string& orig) {
        std::string stringsConfigFile = "";
        std::string platformDir = "/opt/fpp/capes/" + getPlatformCapeDir();
#if defined(PLATFORM_BBB) || defined(PLATFORM_BB64)
        stringsConfigFile = "/home/fpp/media/config/co-bbbStrings.json";
#elif defined(PLATFORM_PI)
        stringsConfigFile = "/home/fpp/media/config/co-pixelStrings.json";
#endif
        if (file_exists(stringsConfigFile)) {
            Json::Value root;
            if (LoadJsonFromFile(stringsConfigFile, root)) {
                if (root["channelOutputs"][0]["enabled"].asInt()) {
                    std::string type = root["channelOutputs"][0]["type"].asString();
                    std::string subtype = root["channelOutputs"][0]["subType"].asString();
                    std::string pinout = "1.x";
                    if (root["channelOutputs"][0].isMember("pinoutVersion")) {
                        pinout = root["channelOutputs"][0]["pinoutVersion"].asString();
                    }
                    std::string key = type + ":" + subtype;
                    std::string newEeprom = CONFIG_EEPROM_UPGRADE_MAP[key];
                    if (pinout == "2.x") {
                        newEeprom += "v2";
                    }
                    if (file_exists(platformDir + "/" + newEeprom + "-eeprom.bin")) {
                        // found a virtual eeprom, install it and use it
                        printf("Installing new virtual eeprom based on prior string configuration: %s\n", newEeprom.c_str());
                        newEeprom = platformDir + "/" + newEeprom + "-eeprom.bin";
                        copyFile(newEeprom, "/home/fpp/media/config/cape-eeprom.bin");
                        setFilePerms("/home/fpp/media/config/cape-eeprom.bin");
                        readOnly = false;
                        return "/home/fpp/media/config/cape-eeprom.bin";
                    }
                }
            }
        }
        return orig;
    }
    void readSettingsFile(std::vector<std::string>& lines) {
        std::string line;
        std::ifstream settingsIstream("/home/fpp/media/settings");
        if (settingsIstream.good()) {
            while (std::getline(settingsIstream, line)) {
                if (line.size() > 0) {
                    lines.push_back(line);
                }
            }
        }
        settingsIstream.close();
    }
    void writeSettingsFile(const std::vector<std::string>& lines) {
        std::ofstream settingsOstream("/home/fpp/media/settings");
        for (auto l : lines) {
            settingsOstream << l << "\n";
        }
        settingsOstream.close();
    }
    void getFileList(const std::string& basepath, const std::string& path, std::vector<std::string>& files) {
        DIR* dir = opendir(basepath.c_str());
        if (dir) {
            files.push_back(path + "/");
            struct dirent* ep;
            while ((ep = readdir(dir))) {
                std::string v = ep->d_name;
                if (v != "." && v != "..") {
                    std::string newBPath = basepath + "/" + v;
                    std::string newPath = path + "/" + v;
                    getFileList(newBPath, newPath, files);
                }
            }
            closedir(dir);
        } else if (file_exists(basepath)) {
            files.push_back(path);
        }
    }
    void DoSignatureVerify(FILE* file, const std::string& fKeyId, const std::string& path, bool& validSignature, uint8_t* signature, int sigLen) {
        uint8_t* b = new uint8_t[32768];
        size_t pos = ftell(file);
        int flen = std::stoi(read_string(file, 6));
        while (flen != 0) {
            int flag = std::stoi(read_string(file, 2));
            if (flag < 50) {
                flen += 64;
            }
            fseek(file, flen, SEEK_CUR);
            flen = std::stoi(read_string(file, 6));
        }
        int end = ftell(file);
        fseek(file, pos, SEEK_SET);
        int len = fread(b, 1, end - pos, file);
        fseek(file, pos, SEEK_SET);

        const uint8_t* key = KEYS[fKeyId].first;
        size_t keyLen = KEYS[fKeyId].second;

#if OPENSSL_VERSION_NUMBER < 0x30000000L
        EC_KEY* pubECKey = nullptr;
        BIO* keybio = BIO_new_mem_buf((void*)key, keyLen);
        pubECKey = PEM_read_bio_EC_PUBKEY(keybio, &pubECKey, NULL, NULL);
        BIO_free(keybio);
        EVP_PKEY* pubKey = EVP_PKEY_new();
        EVP_PKEY_assign_EC_KEY(pubKey, pubECKey);
#else
        EVP_PKEY* pubKey = NULL;
        OSSL_DECODER_CTX* ctx = OSSL_DECODER_CTX_new_for_pkey(&pubKey, "PEM", nullptr,
                                                              "EC",
                                                              0,
                                                              NULL, NULL);
        if (OSSL_DECODER_from_data(ctx, &key, &keyLen) == 0) {
            OSSL_DECODER_CTX_free(ctx);
            validSignature = 0;
            delete[] b;
            return;
        }
        OSSL_DECODER_CTX_free(ctx);
#endif

        EVP_MD_CTX* m_VerifyCtx = EVP_MD_CTX_create();
        EVP_DigestVerifyInit(m_VerifyCtx, NULL, EVP_sha256(), NULL, pubKey);
        EVP_DigestVerifyUpdate(m_VerifyCtx, b, len);
        int AuthStatus = EVP_DigestVerifyFinal(m_VerifyCtx, signature, sigLen);
        EVP_MD_CTX_free(m_VerifyCtx);
        EVP_PKEY_free(pubKey);
        validSignature = AuthStatus == 1;
        delete[] b;
    }

    bool readOnly;
    int bus;
    std::string EEPROM;
    std::string ORIGEEPROM;
    std::string outputPath;

    std::string cape;
    std::string capev;
    std::string capesn;
    std::string devsn;

    std::string fkeyId;
    std::map<std::string, std::vector<std::string>> extras;
    std::map<std::string, std::vector<std::string>> signedExtras;
    bool validSignature = false;
    bool hasSignature = false;
    bool validEpromLocation = true;
    bool corruptEEPROM = false;

    std::map<std::string, std::vector<uint8_t>> fileMap;
    Json::Value capeInfo;
};

CapeUtils CapeUtils::INSTANCE;
CapeUtils::CapeUtils() :
    capeInfo(nullptr) {
}
CapeUtils::~CapeUtils() {
    if (capeInfo) {
        delete capeInfo;
    }
}
CapeInfo* CapeUtils::initCapeInfo(bool ro) {
    if (capeInfo == nullptr) {
        capeInfo = new CapeInfo(ro);
    }
    return capeInfo;
}

CapeUtils::CapeStatus CapeUtils::initCape(bool readOnly) {
    return initCapeInfo(readOnly)->capeStatus();
}

bool CapeUtils::hasFile(const std::string& path) {
    return initCapeInfo(true)->hasFile(path);
}
std::vector<uint8_t> CapeUtils::getFile(const std::string& path) {
    return initCapeInfo(true)->getFile(path);
}
const Json::Value& CapeUtils::getCapeInfo() {
    static Json::Value val;
    initCapeInfo(true);
    val = capeInfo->getCapeInfo();
    return val;
}

int CapeUtils::getLicensedOutputs() {
    return capeInfo->getLicensedOutputs();
}

std::string CapeUtils::getKeyId() {
    return capeInfo->getKeyId();
}

bool CapeUtils::getStringConfig(const std::string& type, Json::Value& val) {
    Json::Value result;
    Json::CharReaderBuilder factory;
    std::string errors;

    std::string fn = "/tmp/strings/" + type + ".json";
    if (hasFile(fn)) {
        const std::vector<uint8_t>& f = getFile(fn);
        std::istringstream istream(std::string((const char*)&f[0], f.size()));
        bool success = Json::parseFromStream(factory, istream, &val, &errors);
        if (success) {
            val["supportsSmartReceivers"] = capeInfo->capeStatus() >= CapeUtils::CapeStatus::SIGNED_GENERIC;
            return true;
        }
    } else {
        fn = "/opt/fpp/capes/";
        fn += getPlatformCapeDir();
        fn += "/strings/" + type + ".json";
        if (file_exists(fn)) {
            std::ifstream istream(fn);
            bool success = Json::parseFromStream(factory, istream, &val, &errors);
            if (success) {
                if (capeInfo->capeStatus() <= CapeUtils::CapeStatus::UNSIGNED) {
                    val["supportsSmartReceivers"] = false;
                }
                return true;
            }
        }
    }
    return false;
}

bool CapeUtils::getPWMConfig(const std::string& type, Json::Value& val) {
    Json::Value result;
    Json::CharReaderBuilder factory;
    std::string errors;

    std::string fn = "/tmp/pwm/" + type + ".json";
    if (hasFile(fn) && getLicensedOutputs() > 0) {
        const std::vector<uint8_t>& f = getFile(fn);
        std::istringstream istream(std::string((const char*)&f[0], f.size()));
        bool success = Json::parseFromStream(factory, istream, &val, &errors);
        if (success) {
            return true;
        }
    }
    return false;
}

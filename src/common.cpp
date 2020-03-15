/*
 *   Common functions for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#ifndef PLATFORM_OSX
#include <linux/if_link.h>
#endif
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ctype.h>
#include <unistd.h>


#include <sstream>
#include <fstream>
#include <algorithm>

#include <curl/curl.h>

#include "common.h"
#include "fppversion_defines.h"
#include "log.h"

/*
 * Get the current time down to the microsecond
 */
long long GetTime(void) {
	struct timeval now_tv;
	gettimeofday(&now_tv, NULL);
	return now_tv.tv_sec * 1000000LL + now_tv.tv_usec;
}

long long GetTimeMS(void) {
    struct timeval now_tv;
    gettimeofday(&now_tv, NULL);
    return now_tv.tv_sec * 1000LL + now_tv.tv_usec / 1000;
}


/*
 * Check to see if the specified directory exists
 */
int DirectoryExists(const char * Directory)
{
	DIR* dir = opendir(Directory);
	if (dir)
	{
		closedir(dir);
		return 1;
	}
	else
	{
		return 0;
	}
}

/*
 * Check if the specified file exists or not
 */
int FileExists(const char * File)
{
	struct stat sts;
	if (stat(File, &sts) == -1) {
		return 0;
	} else {
		return 1;
	}
}
int FileExists(const std::string &f) {
    return FileExists(f.c_str());
}

int Touch(const std::string &File) {
    int fd = open(File.c_str(), O_WRONLY|O_CREAT|O_NOCTTY|O_NONBLOCK, 0666);
    if (fd < 0)
        return 0;

    close(fd);

    return 1;
}


/*
 * Dump memory block in hex and human-readable formats
 */
void HexDump(const char *title, const void *data, int len) {
	int l = 0;
	int i = 0;
	int x = 0;
	int y = 0;
	unsigned char *ch = (unsigned char *)data;
	unsigned char str[17];
	char tmpStr[90];

	sprintf( tmpStr, "%s: (%d bytes)\n", title, len);
	LogInfo(VB_ALL, tmpStr);

	while (l < len) {
		if ( x == 0 ) {
			sprintf( tmpStr, "%06x: ", i );
		}

		if ( x < 16 ) {
			sprintf( tmpStr + strlen(tmpStr), "%02x ", *ch & 0xFF );
			str[x] = *ch;
			x++;
			i++;
		} else {
			sprintf( tmpStr + strlen(tmpStr), "   " );
			for( ; x > 0 ; x-- ) {
                if (str[16-x] == '%' || str[16-x] == '\\') {
                    //these are escapes for the Log call, so don't display them
                    sprintf( tmpStr + strlen(tmpStr), "." );
                } else if ( isgraph( str[16-x] ) || str[16-x] == ' ') {
					sprintf( tmpStr + strlen(tmpStr), "%c", str[16-x]);
				} else {
					sprintf( tmpStr + strlen(tmpStr), "." );
				}
			}

			sprintf( tmpStr + strlen(tmpStr), "\n" );
			LogInfo(VB_ALL, tmpStr);
			x = 0;

			sprintf( tmpStr, "%06x: ", i );
			sprintf( tmpStr + strlen(tmpStr), "%02x ", *ch & 0xFF );
			str[x] = *ch;
			x++;
			i++;
		}

		l++;
		ch++;
	}
	for( y = x; y < 16 ; y++ ) {
		sprintf( tmpStr + strlen(tmpStr), "   " );
	}
	sprintf( tmpStr + strlen(tmpStr), "   " );
	for( y = 0; y < x ; y++ ) {
        if (str[16-x] == '%' || str[16-x] == '\\') {
            //these are escapes for the Log call, so don't display them
            sprintf( tmpStr + strlen(tmpStr), "." );
        } else if ( isgraph( str[y] ) || str[y] == ' ' ) {
			sprintf( tmpStr + strlen(tmpStr), "%c", str[y]);
		} else {
			sprintf( tmpStr + strlen(tmpStr), "." );
		}
	}

	sprintf( tmpStr + strlen(tmpStr), "\n" );
	LogInfo(VB_ALL, tmpStr);
}

/*
 * Get IP address on specified network interface
 */
int GetInterfaceAddress(const char *interface, char *addr, char *mask, char *gw)
{
	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;

	/* I want IP address attached to E131interface */
	strncpy(ifr.ifr_name, (const char *)interface, IFNAMSIZ-1);

	if (addr)
	{
		if (ioctl(fd, SIOCGIFADDR, &ifr))
			strcpy(addr, "127.0.0.1");
		else
			strcpy(addr, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	}

	if (mask)
	{
		if (ioctl(fd, SIOCGIFNETMASK, &ifr))
			strcpy(mask, "255.255.255.255");
		else
			strcpy(mask, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	}

	if (gw)
	{
		FILE *f;
		char  line[100];
		char *iface;
		char *dest;
		char *route;
		char *saveptr;

		f = fopen("/proc/net/route", "r");

		if (f)
		{
			while (fgets(line, 100, f))
			{
				iface = strtok_r(line, " \t", &saveptr);
				dest  = strtok_r(NULL, " \t", &saveptr);
				route = strtok_r(NULL, " \t", &saveptr);

				if ((iface && dest && route) &&
					(!strcmp(iface, interface)) &&
					(!strcmp(dest, "00000000")))
				{
					char *end;
					int ng = strtol(route, &end, 16);
					struct in_addr addr;
					addr.s_addr = ng;
					strcpy(gw, inet_ntoa(addr));
				}
			}
			fclose(f);
		}
	}

	close(fd);

	return 0;
}

/*
 *
 */
char *FindInterfaceForIP(char *ip)
{
	struct ifaddrs *ifaddr, *ifa;
	int family, s, n;
	char host[NI_MAXHOST];
	char interfaceIP[16];
	static char interface[10] = "";

	if (getifaddrs(&ifaddr) == -1)
	{
		LogErr(VB_SETTING, "Error getting interfaces list: %s\n",
			strerror(errno));
		return interface;
	}

	for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++)
	{
		if (ifa->ifa_addr == NULL)
			continue;

		if (ifa->ifa_addr->sa_family == AF_INET)
		{
			s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
							host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			if (s != 0)
			{
				LogErr(VB_SETTING, "getnameinfo failed");
				freeifaddrs(ifaddr);
				return interface;
			}

			if (!strcmp(host, ip))
			{
				strcpy(interface, ifa->ifa_name);
				freeifaddrs(ifaddr);
				return interface;
			}
		}
	}

	return interface;
}

/*
 *
 */
int CheckForHostSpecificFile(const char *hostname, char *filename)
{
    std::string f = filename;
    if (CheckForHostSpecificFile(hostname, f)) {
        strcpy(filename, f.c_str());
        return 1;
    }
    return 0;
}
int CheckForHostSpecificFile(const std::string &hostname, std::string &filename)
{
    std::string localFilename = filename;

	int len = localFilename.length();
    int extIdx = 0;
    
	// Check for 3 or 4-digit extension
    if (localFilename[len - 4] == '.') {
        extIdx = len - 4;
    } else if (localFilename[len - 5] == '.') {
        extIdx = len - 5;
    }

	if (extIdx) {
		// Preserve the extension including the dot
        std::string ext = localFilename.substr(extIdx);
        localFilename = localFilename.substr(0, extIdx);
        localFilename += "-";
        localFilename += hostname;
        localFilename += ext;

		if (FileExists(localFilename)) {
			LogDebug(VB_SEQUENCE, "Found %s to use instead of %s\n",
				localFilename.c_str(), filename.c_str());
            filename = localFilename;
			return 1;
		} else {
			// Replace hyphen with an underscore and recheck
            localFilename[extIdx] = '_';
            if (FileExists(localFilename)) {
				LogDebug(VB_SEQUENCE, "Found %s to use instead of %s\n",
                         localFilename.c_str(), filename.c_str());
				filename = localFilename;
				return 1;
			}
		}
	}

	return 0;
}

static unsigned char bitLookup[16] = {
0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf, };

/*
 * Reverse bits in a byte
 */
uint8_t ReverseBitsInByte(uint8_t n)
{
   return (bitLookup[n & 0b1111] << 4) | bitLookup[n >> 4];
}

/*
 * Convert a string of the form "YYYY-MM-DD to an integer YYYYMMDD
 */
int DateStrToInt(const char *str)
{
	if ((!str) || (str[4] != '-') || (str[7] != '-') || (str[10] != 0x0))
		return 0;

	int result = 0;
	char tmpStr[11];

	strcpy(tmpStr, str);

	result += atoi(str    ) * 10000; // Year
	result += atoi(str + 5) *   100; // Month
	result += atoi(str + 8)        ; // Day

	return result;
}

/*
 * Get the current date in an integer form YYYYMMDD
 */
int GetCurrentDateInt(int daysOffset)
{
	time_t currTime = time(NULL) + (daysOffset * 86400);
	struct tm now;
	int result = 0;
	
	localtime_r(&currTime, &now);

	result += (now.tm_year + 1900) * 10000;
	result += (now.tm_mon + 1)     *   100;
	result += (now.tm_mday)               ;

	return result;
}

/*
 * Close all open file descriptors (used after fork())
 */
void CloseOpenFiles(void)
{
	int maxfd = sysconf(_SC_OPEN_MAX);

	for (int fd = 3; fd < maxfd; fd++)
	    close(fd);
}

/*
 * Check to see if current date int is in the range specified
 */
int CurrentDateInRange(int startDate, int endDate)
{
	int currentDate = GetCurrentDateInt();

	LogExcess(VB_GENERAL, "CurrentDateInRange, checking if %d (s) <= %d (c) <= %d (e)\n", startDate, currentDate, endDate);

	if ((startDate < 10000) || (endDate < 10000))
	{
		startDate = startDate % 10000;
		endDate = endDate % 10000;
		currentDate = currentDate % 10000;

		if (endDate < startDate)
		{
			if (currentDate <= endDate)
				currentDate += 10000;

			endDate += 10000; // next year
		}
	}

	if ((startDate < 100) || (endDate < 100))
	{
		startDate = startDate % 100;
		endDate = endDate % 100;
		currentDate = currentDate % 100;

		if (endDate < startDate)
		{
			if (currentDate <= endDate)
				currentDate += 100;

			endDate += 100; // next month
		}
	}

	LogExcess(VB_GENERAL, "Actual compare is: %d (s) <= %d (c) <= %d (e)\n", startDate, currentDate, endDate);

	if ((startDate == 0) && (endDate == 0))
		return 1;

	if ((startDate <= currentDate) && (currentDate <= endDate))
		return 1;

	return 0;
}

std::string tail(std::string const& source, size_t const length)
{
	if (length >= source.size())
		return source;

	return source.substr(source.size() - length);
}

/*
 * Helpers to split a string on the specified character delimiter
 */
std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;

    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }

    return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;

    split(s, delim, elems);

    return elems;
}

inline std::string dequote(const std::string &s) {
    if ((s[0] == '\'' || s[0] == '"')
        && s[0] == s[s.length() - 1]
        && s.length() > 2) {
        return s.substr(1, s.length() - 2);
    }
    return s;
}

std::vector<std::string> splitWithQuotes(const std::string &s, char delim) {
    std::vector<std::string> ret;
    const char *mystart = s.c_str();
    bool instring = false;
    for (const char* p = mystart; *p; p++) {
        if (*p == '"' || *p == '\'') {
            instring = !instring;
        } else if (*p == delim && !instring) {
            ret.push_back(dequote(std::string(mystart, p - mystart)));
            mystart = p + 1;
        }
    }
    ret.push_back(dequote(std::string(mystart)));
    return ret;
}


std::string GetFileContents(const std::string &filename)
{
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    if (in) {
        std::string contents;
        in.seekg(0, std::ios::end);
        contents.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], contents.size());
        in.close();
        return(contents);
    }
    return "";
}

bool PutFileContents(const std::string &filename, const std::string &str) {
    std::ofstream out(filename, std::ofstream::out);
    if (out) {
        out << str;
        out.close();

        SetFilePerms(filename);

        return true;
    }

    LogErr(VB_GENERAL, "ERROR: Unable to open %s for writing.\n", filename.c_str());

    return false;
}

bool SetFilePerms(const std::string &filename)
{
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    chmod(filename.c_str(), mode);

    struct passwd *pwd = getpwnam("fpp");
    chown(filename.c_str(), pwd->pw_uid, pwd->pw_gid);

    return true;
}

bool SetFilePerms(const char *file)
{
    const std::string filename = file;
    return SetFilePerms(filename);
}

/////////////////////////////////////////////////////////////////////////////
#ifndef PLATFORM_OSX
/*
 * Merge the contens of Json::Value b into Json::Value a
 */
void MergeJsonValues(Json::Value &a, Json::Value &b)
{
	if (!a.isObject() || !b.isObject())
		return;

	Json::Value::Members memberNames = b.getMemberNames();

	for (unsigned int i = 0; i < memberNames.size(); i++)
	{
		std::string key = memberNames[i];

		if ((a[key].type() == Json::objectValue) &&
			(b[key].type() == Json::objectValue))
		{
			MergeJsonValues(a[key], b[key]);
		}
		else
		{
			a[key] = b[key];
		}
	}
}

/*
 *
 */
Json::Value LoadJsonFromFile(const std::string &filename)
{
    Json::Value root;

    LoadJsonFromFile(filename, root);

    return root;
}

/*
 *
 */
Json::Value LoadJsonFromString(const std::string &str) {
    Json::Value root;
    bool result = LoadJsonFromString(str, root);

    return root;
}

/*
 *
 */
bool LoadJsonFromString(const std::string &str, Json::Value &root)
{
    Json::CharReaderBuilder builder;
    Json::CharReader *reader = builder.newCharReader();
    std::string errors;

    builder["collectComments"] = false;

    bool success = reader->parse(str.c_str(), str.c_str() + str.size(), &root, &errors);
    delete reader;

    if (!success) {
        LogErr(VB_GENERAL, "Error parsing JSON string in LoadJsonFromString(): '%s'\n", str.c_str());
        Json::Value empty;
        root = empty;
        return false;
    }

    LogDebug(VB_GENERAL, "LoadJsonFromString() loaded: '%s'\n", str.c_str());

    return true;
}

bool LoadJsonFromFile(const std::string &filename, Json::Value &root)
{
    if (!FileExists(filename)) {
        LogErr(VB_GENERAL, "JSON File %s does not exist\n", filename.c_str());
        return false;
    }

    std::string jsonStr = GetFileContents(filename);

    return LoadJsonFromString(jsonStr, root);
}

bool LoadJsonFromFile(const char *filename, Json::Value &root)
{
    std::string filenameStr = filename;

    return LoadJsonFromFile(filenameStr, root);
}

std::string SaveJsonToString(const Json::Value &root, const std::string &indentation) {
    Json::StreamWriterBuilder wbuilder;
    wbuilder["indentation"] = indentation;

    std::string result = Json::writeString(wbuilder, root);

    return result;
}

bool SaveJsonToString(const Json::Value &root, std::string &str, const std::string &indentation) {
    str = SaveJsonToString(root, indentation);

    if (str.empty())
        return false;

    return true;
}

bool SaveJsonToFile(const Json::Value &root, const std::string &filename, const std::string &indentation) {
    std::string str;

    bool result = SaveJsonToString(root, str, indentation);

    if (!result) {
        LogErr(VB_GENERAL, "Error converting Json::Value to std::string()\n");
        return false;
    }

    result = PutFileContents(filename, str);
    if (!result) {
        return false;
    }

    return true;
}

bool SaveJsonToFile(const Json::Value &root, const char *filename, const char *indentation) {
    std::string filenameStr = filename;
    std::string indentationStr = indentation;

    return SaveJsonToFile(root, filenameStr, indentationStr);
}
#endif


/////////////////////////////////////////////////////////////////////////////
// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}
// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}
// trim from both ends (in place)
void TrimWhiteSpace(std::string &s) {
    ltrim(s);
    rtrim(s);
}

bool startsWith(const std::string &str, const std::string &prefix) {
    return ((prefix.size() <= str.size()) && std::equal(prefix.begin(), prefix.end(), str.begin()));
}
bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && 0 == str.compare(str.size()-suffix.size(), suffix.size(), suffix);
}
bool contains(const std::string &str, const std::string &v) {
    return str.find(v) != std::string::npos;
}
void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // ...
    }
}

bool replaceStart(std::string& str, const std::string& from, const std::string& to) {
    if (!startsWith(str, from))
        return false;

    str.replace(0, from.size(), to);
    return true;
}

bool replaceEnd(std::string& str, const std::string& from, const std::string& to) {
    if (!endsWith(str, from))
        return false;

    str.replace(str.size()-from.size(), from.size(), to);
    return true;
}

void toUpper(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
}
void toLower(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
}
std::string toUpperCopy(const std::string& str) {
    std::string cp = str;
    toUpper(cp);
    return cp;
}
std::string toLowerCopy(const std::string& str) {
    std::string cp = str;
    toLower(cp);
    return cp;
}


// URL Helpers
size_t urlWriteData(void *buffer, size_t size, size_t nmemb, void *userp)
{
	std::string *str = (std::string *)userp;

	str->append(static_cast<const char*>(buffer), size * nmemb);

	return size * nmemb;
}

bool urlHelper(const std::string method, const std::string &url, const std::string &data, std::string &resp)
{
	CURL *curl = curl_easy_init();
	struct curl_slist *headers = NULL;
	std::string userAgent = "FPP/" FPP_SOURCE_VERSION_STR;

	resp = "";

	if (!curl)
	{
		LogDebug(VB_GENERAL, "Unable to create curl instance in urlHelper()\n");
		return false;
	}

	CURLcode status;
	status = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, urlWriteData);
	if (status != CURLE_OK)
	{
		LogErr(VB_GENERAL, "curl_easy_setopt() Error setting write callback function: %s\n", curl_easy_strerror(status));
		return false;
	}

	status = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
	if (status != CURLE_OK)
	{
		LogErr(VB_GENERAL, "curl_easy_setopt() Error setting class pointer: %s\n", curl_easy_strerror(status));
		return false;
	}

	status = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	if (status != CURLE_OK)
	{
		LogErr(VB_GENERAL, "curl_easy_setopt() Error setting URL: %s\n", curl_easy_strerror(status));
		return false;
	}

	if (startsWith(data,"{") && endsWith(data,"}"))
	{
		headers = curl_slist_append(headers, "Accept: application/json");
		headers = curl_slist_append(headers, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}

	if (data != "")
	{
		status = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
		if (status != CURLE_OK)
		{
			LogErr(VB_GENERAL, "curl_easy_setopt() Error setting postfields data: %s\n", curl_easy_strerror(status));
			return false;
		}
	}

	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

	if (method == "POST")
		curl_easy_setopt(curl, CURLOPT_POST, 1);
	else if (method == "PUT")
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
	else if (method == "DELETE")
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

	curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());

	status = curl_easy_perform(curl);
	if (status != CURLE_OK)
	{
		LogErr(VB_GENERAL, "curl_easy_perform() failed: %s\n", curl_easy_strerror(status));
		return false;
	}

	LogDebug(VB_GENERAL, "resp: %s\n", resp.c_str());

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	return true;
}

bool urlHelper(const std::string method, const std::string &url, std::string &resp)
{
	std::string data;
	return urlHelper(method, url, data, resp);
}

bool urlGet(const std::string url, std::string &resp)
{
	std::string data;
	return urlHelper("GET", url, resp);
}

bool urlPost(const std::string url, const std::string data, std::string &resp)
{
	return urlHelper("POST", url, data, resp);
}

bool urlPut(const std::string url, const std::string data, std::string &resp)
{
	return urlHelper("PUT", url, data, resp);
}

bool urlDelete(const std::string url, const std::string data, std::string &resp)
{
	return urlHelper("DELETE", url, data, resp);
}

bool urlDelete(const std::string url, std::string &resp)
{
	std::string data;
	return urlHelper("DELETE", url, data, resp);
}


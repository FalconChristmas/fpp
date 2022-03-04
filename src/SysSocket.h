#pragma once

#include <sys/socket.h>


#ifdef PLATFORM_OSX

#ifndef SOCK_NONBLOCK
#include <fcntl.h>
//# define SOCK_NONBLOCK O_NONBLOCK
#define SOCK_NONBLOCK 0
#endif

#include <libkern/OSByteOrder.h>

#define htobe16(x) OSSwapHostToBigInt16(x)
#define htole16(x) OSSwapHostToLittleInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)

#define htobe32(x) OSSwapHostToBigInt32(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)

#define htobe64(x) OSSwapHostToBigInt64(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)

struct mmsghdr {
    struct msghdr   msg_hdr;
    unsigned int        msg_len;
};

struct sockaddr_ll {
    unsigned short sll_family;   /* Always AF_PACKET */
    unsigned short sll_protocol; /* Physical-layer protocol */
    int            sll_ifindex;  /* Interface number */
    unsigned short sll_hatype;   /* ARP hardware type */
    unsigned char  sll_pkttype;  /* Packet type */
    unsigned char  sll_halen;    /* Length of address */
    unsigned char  sll_addr[8];  /* Physical-layer address */
};

struct ether_header {
    u_char    ether_dhost[6];
    u_char    ether_shost[6];
    u_short   ether_type;
};

inline int recvmmsg(int fd, struct mmsghdr *msgs, unsigned int n,
                    unsigned int flags, struct timespec *timeout)
{
    for (unsigned int i = 0; i < n; i++) {
        ssize_t retval = recvmsg(fd, &msgs[i].msg_hdr, flags);
        if (retval < 0) {
            return i ? i : retval;
        }
        msgs[i].msg_len = retval;
    }
    return n;
}


inline int sendmmsg(int fd, struct mmsghdr *msgs,
                    unsigned int n, unsigned int flags)
{
    for (unsigned int i = 0; i < n; i++) {
        ssize_t retval = sendmsg(fd, &msgs[i].msg_hdr, flags);
        if (retval < 0) {
            return i ? i : retval;
        }
        msgs[i].msg_len = retval;
    }
    return n;
}

#endif

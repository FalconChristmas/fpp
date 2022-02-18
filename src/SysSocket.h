#pragma once

#include <sys/socket.h>


#ifdef PLATFORM_OSX

#ifndef SOCK_NONBLOCK
#include <fcntl.h>
//# define SOCK_NONBLOCK O_NONBLOCK
#define SOCK_NONBLOCK 0
#endif

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

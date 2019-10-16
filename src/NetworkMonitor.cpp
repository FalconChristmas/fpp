/*
*   Falcon Player Daemon
*
*   Copyright (C) 2013-2019 the Falcon Player Developers
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

#include <net/if.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/types.h>
#include <unistd.h>

#include "log.h"
#include "common.h"

#include "NetworkMonitor.h"

NetworkMonitor NetworkMonitor::INSTANCE;

void NetworkMonitor::Init(std::map<int, std::function<bool(int)>> &callbacks) {
    
    int nl_socket = socket (AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (nl_socket < 0) {
        LogWarn(VB_GENERAL, "Could not create NETLINK socket.\n");
    }

    struct sockaddr_nl addr;
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid ();
    addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;
    
    if (bind(nl_socket, (struct sockaddr *) &addr, sizeof (addr)) < 0) {
        LogWarn(VB_GENERAL, "Could not bind NETLINK socket.\n");
    }
    callbacks[nl_socket] = [nl_socket, this](int i) {
        int status;
        int ret = 0;
        char buf[4096];
        struct iovec iov = { buf, sizeof buf };
        struct sockaddr_nl snl;
        struct msghdr msg = { (void *) &snl, sizeof snl, &iov, 1, NULL, 0, 0 };
        struct nlmsghdr *h;
        char name[IF_NAMESIZE + 1];

        status = recvmsg(nl_socket, &msg, MSG_DONTWAIT);
        while (status > 0) {
            bool OK = true;
            for (h = (struct nlmsghdr *) buf; OK && NLMSG_OK (h, (unsigned int) status); h = NLMSG_NEXT (h, status)) {
                //Finish reading
                switch (h->nlmsg_type) {
                    case NLMSG_DONE:
                        OK = false;
                        break;
                    case NLMSG_ERROR:
                        //error - not sure what to do, just bail
                        OK = false;
                        break;
                    case RTM_NEWLINK:
                    case RTM_DELLINK:
                        {
                            struct ifinfomsg *ifi = (ifinfomsg*)NLMSG_DATA(h);
                            callCallbacks(h->nlmsg_type == RTM_NEWLINK ? NetEventType::NEW_LINK : NetEventType::DEL_LINK,
                                          (ifi->ifi_flags & IFF_RUNNING) ? 1 : 0,
                                          if_indextoname(ifi->ifi_index, name));
                        }
                        break;
                    case RTM_NEWADDR:
                    case RTM_DELADDR:
                        {
                            struct ifaddrmsg *ifi = (ifaddrmsg*)NLMSG_DATA(h);
                            if (ifi->ifa_family == AF_INET) {
                                callCallbacks(h->nlmsg_type == RTM_NEWADDR ? NetEventType::NEW_ADDR : NetEventType::DEL_ADDR,
                                              h->nlmsg_type == RTM_NEWADDR ? 1 : 0,
                                              if_indextoname(ifi->ifa_index, name));
                            }
                        }
                        break;
                    default:
                        //printf("NETLINK: %d   Uknown\n", h->nlmsg_type);
                        break;
                }
            }
            status = recvmsg(nl_socket, &msg, MSG_DONTWAIT);
        }
        return false;
    };
}
void NetworkMonitor::callCallbacks(NetEventType nl, int up, const std::string &n) {
    for (auto &cb : callbacks) {
        cb(nl, up, n);
    }
}


void NetworkMonitor::registerCallback(std::function<void(NetEventType, int, const std::string &)> &callback) {
    callbacks.push_back(callback);
}


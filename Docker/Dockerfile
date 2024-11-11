FROM debian:bookworm

# if we get a bunch of these installed up front in one shot (assume a desktop so plenty of memory to do all at once)
# then the FPP_Install step is faster.   In addition, docker will cache this result
# so building docker containers will be faster
RUN DEBIAN_FRONTEND=noninteractive apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -yq upgrade

ARG PHPVER="8.2"

#
# This should exact match SD/FPP_Install.sh
# 
# Packages custom to docker are listed below
#
ARG PACKAGE_LIST="alsa-utils arping avahi-daemon avahi-utils locales nano net-tools \
    apache2 apache2-bin apache2-data apache2-utils libavahi-client-dev \
    bc bash-completion btrfs-progs exfat-fuse lsof ethtool curl zip unzip bzip2 wireless-tools dos2unix \
    fbi fbset file flite ca-certificates lshw gettext wget iproute2 \
    build-essential ffmpeg gcc g++ gdb vim vim-common bison flex device-tree-compiler dh-autoreconf \
    git git-core hdparm i2c-tools ifplugd jq less sysstat tcpdump time usbutils usb-modeswitch \
    samba rsync sudo shellinabox dnsmasq hostapd vsftpd ntp sqlite3 at haveged samba samba-common-bin \
    mp3info exim4 mailutils dhcp-helper parprouted bridge-utils libiio-utils libfmt9 \
    php${PHPVER} php${PHPVER}-cli php${PHPVER}-fpm php${PHPVER}-common php${PHPVER}-curl php-pear \
    php${PHPVER}-bcmath php${PHPVER}-sqlite3 php${PHPVER}-zip php${PHPVER}-xml \
    libavcodec-dev libavformat-dev libswresample-dev libswscale-dev libavdevice-dev libavfilter-dev libtag1-dev \
    vorbis-tools libgraphicsmagick++1-dev graphicsmagick-libmagick-dev-compat libmicrohttpd-dev \
    gettext apt-utils x265 libtheora-dev libvorbis-dev libx265-dev iputils-ping mp3gain \
    libmosquitto-dev mosquitto-clients mosquitto libzstd-dev lzma zstd gpiod libgpiod-dev libjsoncpp-dev libcurl4-openssl-dev \
    fonts-freefont-ttf flex bison pkg-config libasound2-dev mesa-common-dev qrencode libusb-1.0-0-dev \
    flex bison pkg-config libasound2-dev python3-setuptools libssl-dev libtool bsdextrautils iw rsyslog tzdata ccache"

ARG PACKAGE_LIST_DOCKER=ssh

RUN echo "force-unsafe-io" > /etc/dpkg/dpkg.cfg.d/02apt-speedup
RUN echo '\
    Acquire::http {No-Cache=True;};\
    Acquire::https {No-Cache=True;};\
    Acquire::Retries "32";\
    Acquire::https::Timeout "240";\
    Acquire::http::Timeout "240";\
    APT::Get::Assume-Yes "true";\
    APT::Install-Recommends "false";\
    APT::Install-Suggests "false";\
    Debug::Acquire::https "true";\
    ' > /etc/apt/apt.conf.d/99custom

RUN DEBIAN_FRONTEND=noninteractive apt-get install -yq -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" $PACKAGE_LIST

#
# These are unique to docker
#
RUN DEBIAN_FRONTEND=noninteractive apt-get install -yq -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" $PACKAGE_LIST_DOCKER

RUN apt-get clean

# similar to above, do this once and Docker can cache the intermediary
COPY SD/buildVLC.sh /root/buildVLC.sh
RUN ( yes | /root/buildVLC.sh ) || true


# Pass --build-arg EXTRA_INSTALL_FLAG=--skip-clone to build without
# cloning the lastest from git and instead use the stuff in the local
# directory
ARG EXTRA_INSTALL_FLAG=

# branch being build
ARG FPPBRANCH

COPY ./ /opt/fpp/
COPY SD/FPP_Install.sh /root/FPP_Install.sh
RUN ( yes | bash /root/FPP_Install.sh $EXTRA_INSTALL_FLAG --branch ${FPPBRANCH:-master} --skip-apt-install --skip-vlc )
RUN rm -rf /tmp/*
RUN cp /opt/fpp/etc/asoundrc.plain /root/.asoundrc

# this will do additional updates and create the required directories
# and set permissions
RUN /opt/fpp/src/fppinit start

VOLUME /home/fpp/media

#      HTTP  DDP      e1.31    Multisync  FPPD/HTTP    Other
EXPOSE 80    4048/udp 5568/udp 32320/udp    32322      9000/udp 9000/tcp
ENTRYPOINT ["/opt/fpp/Docker/runDocker.sh"]

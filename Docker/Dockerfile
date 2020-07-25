FROM debian:buster

# if we get a bunch of these installed up front in one shot (assume a desktop so plenty of memory to do all at once)
# then the FPP_Install step is faster.   In addition, docker will cache this result
# so building docker containers will be faster
RUN apt-get update ; DEBIAN_FRONTEND=noninteractive apt-get install -yq -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold"  alsa-utils arping avahi-daemon avahi-utils locales nano net-tools \
    apache2 apache2-bin apache2-data apache2-utils libapache2-mod-php \
    bc bash-completion btrfs-progs exfat-fuse lsof ethtool curl zip unzip bzip2 wireless-tools dos2unix \
    fbi fbset file flite ca-certificates lshw \
    build-essential gcc g++ gdb ccache vim vim-common bison flex device-tree-compiler dh-autoreconf \
    git git-core hdparm i2c-tools ifplugd less sysstat tcpdump time usbutils usb-modeswitch \
    samba rsync connman sudo shellinabox dnsmasq hostapd vsftpd ntp sqlite3 at haveged samba samba-common-bin \
    mp3info mailutils dhcp-helper parprouted bridge-utils libiio-utils \
    php php-cli php-common php-curl php-pear php-sqlite3 php-zip php-xml \
    libavcodec-dev libavformat-dev libswresample-dev libswscale-dev libavdevice-dev libavfilter-dev libtag1-dev \
    vorbis-tools libgraphicsmagick++1-dev graphicsmagick-libmagick-dev-compat libmicrohttpd-dev \
    libmosquitto-dev mosquitto-clients mosquitto libzstd-dev lzma zstd gpiod libgpiod-dev libjsoncpp-dev libcurl4-openssl-dev \
    python-daemon python-smbus wget flex bison pkg-config libasound2-dev ; apt-get clean

# similar to above, do this once and Docker can cache the intermediary
ADD SD/buildVLC.sh /root/buildVLC.sh
RUN ( yes | /root/buildVLC.sh ) || true


# Pass --build-arg EXTRA_INSTALL_FLAG=--skip-clone to build without
# cloning the lastest from git and instead use the stuff in the local
# directory
ARG EXTRA_INSTALL_FLAG=

ADD ./ /opt/fpp/
ADD SD/FPP_Install.sh /root/FPP_Install.sh
RUN ( yes | /root/FPP_Install.sh $EXTRA_INSTALL_FLAG --skip-apt-install --skip-vlc ) || true

# this will do additional updates and create the required directories
# and set permissions
RUN /opt/fpp/scripts/init_pre_network

VOLUME /home/fpp/media

#      HTTP  DDP      e1.31    Multisync  FPPD/HTTP    Other
EXPOSE 80    4048/udp 5568/udp 32320/udp    32322      9000/udp 9000/tcp
ENTRYPOINT ["/opt/fpp/Docker/runDocker.sh"]

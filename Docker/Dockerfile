FROM debian:buster


RUN apt-get update

# if we get a bunch of these installed up front in one shot (assume a desktop so plenty of memory to do all at once)
# then the FPP_Install step is faster
RUN DEBIAN_FRONTEND=noninteractive apt-get install -yq -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold"  g++ gcc build-essential libgtk2.0-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev freeglut3-dev libavcodec-dev libavformat-dev libswscale-dev libsdl2-dev libswresample-dev liblog4cpp5-dev libavutil-dev libavresample-dev libportmidi-dev wget git fuse gpgv colormake libzstd-dev sudo alsa-utils arping avahi-daemon \
    apache2 apache2-bin apache2-data apache2-utils \
    zlib1g-dev libpcre3 libpcre3-dev libbz2-dev libssl-dev \
    avahi-discover avahi-utils bash-completion bc btrfs-tools build-essential \
    bzip2 ca-certificates ccache connman curl device-tree-compiler \
    dh-autoreconf ethtool exfat-fuse fbi fbset file flite gdb \
    gdebi-core git git-core hdparm i2c-tools ifplugd less \
    libgraphicsmagick++1-dev graphicsmagick-libmagick-dev-compat \
    libboost-filesystem-dev libboost-system-dev libboost-iostreams-dev libboost-date-time-dev \
    libboost-atomic-dev libboost-math-dev libboost-signals-dev libconvert-binary-c-perl \
    libdbus-glib-1-dev libdevice-serialport-perl libjs-jquery \
    libjs-jquery-ui libjson-perl libjsoncpp-dev liblo-dev libmicrohttpd-dev libnet-bonjour-perl \
    libsdl2-dev libssh-4 libtagc0-dev libtest-nowarnings-perl locales lsof \
    mp3info mailutils mpg123 mpg321 mplayer nano net-tools ntp \
    php-cli php-common php-curl php-fpm php-xml \
    php-sqlite3 php-zip python-daemon python-smbus rsync samba \
    samba-common-bin shellinabox sudo sysstat tcpdump time vim \
    vim-common vorbis-tools vsftpd \
    dhcp-helper hostapd parprouted bridge-utils cpufrequtils \
    dos2unix libmosquitto-dev mosquitto-clients librtmidi-dev \
    libavcodec-dev libavformat-dev libswresample-dev libsdl2-dev libswscale-dev libavdevice-dev libavfilter-dev \
    wireless-tools libcurl4-openssl-dev sqlite3 \
    libzstd-dev
RUN apt-get clean

RUN wget -O /root/FPP_Install.sh https://raw.githubusercontent.com/FalconChristmas/fpp/master/SD/FPP_Install.sh
RUN chmod 700 /root/FPP_Install.sh
RUN ( yes | /root/FPP_Install.sh ) || true

RUN /opt/fpp/scripts/init_pre_network



EXPOSE 80
# DDP
EXPOSE 4048/udp
#e1.31
EXPOSE 5568/udp
# Multisync/discovery
EXPOSE 32320/udp
CMD ["/opt/fpp/Docker/runDocker.sh"]

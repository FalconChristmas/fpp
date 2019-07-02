#!/bin/bash

# build the docker image
docker build --no-cache -t falconchristmas/fpp:latest -f Docker/Dockerfile .
#or
#docker build -t falconchristmas/fpp:latest -f Docker/Dockerfile .




#to run the image using the local content to overwride:
# On OSX, it's highly suggested to set an env variable of:
# export DOCKER_MOUNT_FLAG=":delegated"
# Using :delegated will drop the "make" time by almost half.
# However, editing files from OSX may take a few
# extra second before the changes are available within the docker container.

# docker run --rm -t -i -p 80:80 -p 4048:4048/udp -p 5568:5568/udp -p 32320:32320/udp -v ${PWD}:/opt/fpp${DOCKER_MOUNT_FLAG} falconchristmas/fpp:latest

#!/bin/bash

# build the docker image
docker build -t falconchristmas/fpp:latest -f Docker/Dockerfile .
#or
#docker build -t falconchristmas/fpp:latest  --build-arg EXTRA_INSTALL_FLAG=--skip-clone -f Docker/Dockerfile .




#to run the image using the local content to override:
# On OSX, it's highly suggested to set an env variable of:
# export DOCKER_MOUNT_FLAG=":delegated"
# Using :delegated will drop the "make" time by almost half.
# However, editing files from OSX may take an extra second
# before the changes are available within the docker container.

# docker run --name fpp --hostname fppDocker -t -i -p 80:80 -p 4048:4048/udp -p 5568:5568/udp -p 32320:32320/udp -v ${PWD}:/opt/fpp${DOCKER_MOUNT_FLAG} falconchristmas/fpp:latest

# A sample docker-compose.yml file to use the docker image is also
# included in the Docker directory.

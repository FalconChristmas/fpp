#!/bin/bash

# build the docker image
docker build --no-cache -t fpp:master -f Docker/Dockerfile .

# to push to dockerhub:
#docker build -t dkulp/fpp:master -f Docker/Dockerfile .

#to run the image using the local content to overwride:
# docker run --rm -t -i -p 80:80 -p 4048:4048/udp -p 5568:5568/udp -p 32320:32320/udp -v ${PWD}:/opt/fpp fpp:master

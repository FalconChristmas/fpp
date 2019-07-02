#!/bin/bash

# build the docker image
docker build --no-cache -t fpp:master -f Docker/Dockerfile .


#to run the image using the local content to overwride:
# docker run --rm -t -i  -v ${PWD}:/opt/fpp fpp:master

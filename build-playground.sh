#!/usr/bin/env bash

docker build -t inferno-docker -f playground/Dockerfile .

# start the development container with:
# docker run -p 6080:6080 -p 3000:3000 --mount type=tmpfs,destination=/tmp inferno-docker

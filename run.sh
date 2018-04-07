#!/bin/bash

docker build -t relative-value-img . # build image, tag with relative-value-img
docker run -dit --restart unless-stopped -p 52000:3306 --name relative-value-container relative-value-img # run container from image
# if using gdb, need to add `--cap-add=SYS_PTRACE --security-opt seccomp=unconfined` to docker run, see https://stackoverflow.com/a/46676907/2624391

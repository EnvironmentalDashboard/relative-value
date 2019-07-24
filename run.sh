#!/bin/bash

docker run -dit --restart always --name relative-value-container relative-value-img # run container from image
# if using gdb, need to add `--cap-add=SYS_PTRACE --security-opt seccomp=unconfined` to docker run, see https://stackoverflow.com/a/46676907/2624391

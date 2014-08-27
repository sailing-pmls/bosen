B1;3201;0c#!/bin/sh
set -u

/share/probe/bin/probe-makebed \
  -p BigLearning \
  -e strads8 \
  -i strADS_modify5 \
  -s /users/jinkyuk/test-startup\
  -n $1
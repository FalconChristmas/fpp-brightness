#!/bin/bash

# fpp-brightness install script

BASEDIR=$(dirname $0)
cd $BASEDIR
cd ..
make


. /opt/fpp/scripts/common
setSetting restartFlag 1

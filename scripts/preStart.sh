#!/bin/sh

echo "Running fpp-brightness PreStart Script"

BASEDIR=$(dirname $0)
cd $BASEDIR
cd ..
make

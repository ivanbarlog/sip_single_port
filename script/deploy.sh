#!/bin/bash

if [ "$EUID" -ne 0 ]; then
  echo "Please run as root"
  exit
fi

HOST=${1-"172.16.97.1"}

killall kamailio
rm -rf ./modules/sip_single_port/*
scp -r ivan@${HOST}:/usr/local/src/kamailio/modules/sip_single_port/* ./modules/sip_single_port/
make all
make install
kamailio
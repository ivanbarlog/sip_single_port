#!/bin/bash

# Installing Kamailio - http://www.kamailio.org/wiki/install/4.1.x/git


if [ "$EUID" -ne 0 ]; then 
  echo "Please run as root"
  exit
fi

apt-get install git-core gcc flex bison libmysqlclient-dev make

mkdir -p /usr/local/src
cd /usr/local/src

mkdir kamailio
chown $SUDO_USER kamailio
sudo -u $SUDO_USER git clone --depth 1 git@github.com:kamailio/kamailio.git kamailio
cd kamailio/modules

sudo -u $SUDO_USER git clone git@github.com:ivanbarlog/sip_single_port.git sip_single_port
cd ..

make include_modules="db_mysql sip_single_port nathelper" cfg
make all

make install

#!/bin/bash

# Installing Kamailio - http://www.kamailio.org/wiki/install/4.1.x/git


if [ "$EUID" -ne 0 ]; then 
  echo "Please run as root"
  exit
fi

# install requirements
apt-get install git-core gcc flex bison libmysqlclient-dev make

# prepare source directory
mkdir -p /usr/local/src
cd /usr/local/src

# clone kamailio
mkdir kamailio
chown $SUDO_USER kamailio
sudo -u $SUDO_USER git clone --depth 1 git@github.com:kamailio/kamailio.git kamailio
cd kamailio/modules

# clone sip_single_port module
sudo -u $SUDO_USER git clone git@github.com:ivanbarlog/sip_single_port.git sip_single_port
cd ..

# add necessary modules to config; compile & install
make include_modules="db_mysql sip_single_port nathelper" cfg
make all
make install

# create mysql database
sed -i -e "s/# SIP_DOMAIN=kamailio.org/SIP_DOMAIN=barlog.sk/;s/# DBENGINE=MYSQL/DBENGINE=MYSQL/;s/# ALIASES_TYPE=\"DB\"/ALIASES_TYPE=\"DB\"/" /usr/local/etc/kamailio/kamctlrc
sudo -u $SUDO_USER kamdbctl create

# create users
echo "kamailio password for MySQL is 'kamailiorw'"
sudo -u $SUDO_USER kamctl add bob Bob123
sudo -u $SUDO_USER kamctl add alice Alice123

# add modules to local config
cd /usr/local/etc/kamailio/
touch kamailio-local.cfg
echo -e "#!define WITH_MYSQL\n#!define WITH_AUTH\n#!define WITH_ALIASDB\n#!define WITH_USRLOCDB\n#!define WITH_SIP_SINGLE_PORT\n" > kamailio-local.cfg
echo -e "\n#!ifdef WITH_SIP_SINGLE_PORT\nloadmodule \"sip_single_port.so\"\n#!endif\n" >> kamailio-local.cfg

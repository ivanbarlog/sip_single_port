#~/bin/bash

# fix kamailio sock error when sock was badly configured in kamailio.cfg

mkdir -p /var/run/kamailio && chown kamailio:kamailio /var/run/kamailio

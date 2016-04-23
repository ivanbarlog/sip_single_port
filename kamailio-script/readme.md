In order to access this machine comfortably, please connect to it via SSH from your local host.

If there is problem with internet connection in this machine the following command most likely resolves it:

```
dhclient eth0
```

# Changing IP address of machine

in case that your local network changed be sure that you change the address in all of these places:

- `/usr/local/etc/kamailio/kamctlrc` - domain address
- `/usr/local/etc/kamailio/kamailio-local.cfg` - address of next hop
- `/usr/local/etc/kamailio/kamailio.cfg` - listenin address
- `/etc/networking/interfaces` - static address of this machine
- `~/.ssh/config` - address of your local host with which you are accessing this machine over SSH



# Mounting folders

in order to mount folders from localhost to virtual machine you need to create following symlinks in your local machine since the mounting scripts depends on these locations:

/var/kamailio_src -> links to kamailio source code
/var/kamailio_src/modules/sip_single_port -> links to `sip_single_port` folder from ivanbarlog/sip_single_port repository
/var/kamailio_config -> links to `kamailio-config` folder from ivanbarlog/sip_single_port repository
/var/kamailio_php -> links to `php-script` folder from ivanbarlog/sip_single_port repository

config files from /var/kamailio_config are mounted by hostname of virtual machine so it is crucial to have hostname set so it matches existing folder

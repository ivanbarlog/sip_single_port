#!/bin/bash

sshfs kamailioA:/root/kamailio_src/ kamailioA_src/ -oauto_cache,reconnect,no_readahead,follow_symlinks,transform_symlinks
sshfs kamailioA:/root/kamailio_etc/ kamailioA_etc/ -oauto_cache,reconnect,no_readahead,follow_symlinks,transform_symlinks
sshfs kamailioB:/root/kamailio_src/ kamailioB_src/ -oauto_cache,reconnect,no_readahead,follow_symlinks,transform_symlinks
sshfs kamailioB:/root/kamailio_etc/ kamailioB_etc/ -oauto_cache,reconnect,no_readahead,follow_symlinks,transform_symlinks

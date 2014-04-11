#!/bin/sh
#
# Install system configuration and pre-built binaries
# (x86 Linux)
#

sudo apt-get install supervisor lighttpd
sudo cp -r etc usr /
sudo supervisorctl reload


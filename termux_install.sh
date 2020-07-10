#!/bin/sh

###
# Script for installing tekstitv in termux
# Usage:
# ./termux_install.sh [option]
# Options:
#   install     install the program [default]
#   reinstall   rebuild from scratch and install
#   update      fetch the latest changes and install
#   uninstall   remove installed binary
##

set -e

dependency() {
    pkg install ncurses libcurl
}

install() {
    dependency
    ./configure --disable-lib-build --termux-build
    make
    CPWD=`pwd`
    cd ~
    cp "$CPWD/build/tekstitv" ../usr/bin
    cd $CPWD
    exit 0
}

update() {
    git pull
    install
}

reinstall() {
    make clean
    install
}

uninstall() {
    CPWD=`pwd`
    cd ~
    rm ../usr/bin/tekstitv
    cd $CPWD
    exit 0
}

# check the option
case "$1" in
    "update")
        update
        ;;
    "install")
        install
        ;;
    "reinstall")
        reinstall
        ;;
    "uninstall")
        uninstall
        ;;
esac

# Install by default
install

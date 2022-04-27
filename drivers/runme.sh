#! /bin/bash

MODULE="cryptocard_mod"

sudo rmmod $MODULE
sudo dmesg -c
make && sudo insmod ${MODULE}.ko

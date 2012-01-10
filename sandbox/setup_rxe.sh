#!/bin/sh

sudo modprobe rmda_ucm
sudo modprobe ib_rxe
sudo modprobe ib_rxe_net
sudo rxe_cfg add eth0

#!/bin/bash
set -e

# TUNデバイスの設定
mkdir -p /dev/net
mknod /dev/net/tun c 10 200
chmod 666 /dev/net/tun
ip tuntap add mode tap dev tap0
ip addr add 192.0.2.1/24 dev tap0
ip link set tap0 up

exec "$@"

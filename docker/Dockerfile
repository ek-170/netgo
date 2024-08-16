FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
  iproute2 \
  tcpdump \
  net-tools \
  golang-go \
  sudo \
  make \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/* \
  && useradd -m tcp

USER tcp
WORKDIR /home/tcp
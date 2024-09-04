FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
  build-essential \
  golang-go \
  iproute2 \
  iputils-ping \
  make \
  man-db \
  net-tools \
  netcat-openbsd \
  sudo \
  tcpdump \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /home/tcp
COPY ./docker-entrypoint.sh /

ENTRYPOINT ["/docker-entrypoint.sh"]
CMD [ "bash" ]

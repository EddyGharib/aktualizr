FROM ubuntu:jammy
LABEL Description="Aktualizr dockerfile for Ubuntu Jammy"

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get -y install --no-install-suggests --no-install-recommends \
  apt-transport-https \
  autoconf \
  asn1c \
  automake \
  awscli \
  bison \
  ccache \
  cmake \
  curl \
  e2fslibs-dev \
  fiu-utils \
  g++ \
  gcc \
  git \
  jq \
  lcov \
  libarchive-dev \
  libboost-dev \
  libboost-log-dev \
  libboost-program-options-dev \
  libboost-system-dev \
  libboost-test-dev \
  libboost-thread-dev \
  libcurl4-openssl-dev \
  libengine-pkcs11-openssl \
  libexpat1-dev \
  libffi-dev \
  libfiu-dev \
  libfuse-dev \
  libglib2.0-dev \
  libgpgme11-dev \
  libgtest-dev \
  liblzma-dev \
  libp11-3 \
  libp11-dev \
  libsodium-dev \
  libsqlite3-dev \
  libssl-dev \
  libtool \
  lshw \
  make \
  ninja-build \
  opensc \
  net-tools \
  pkg-config \
  psmisc \
  python3-dev \
  python3-gi \
  python3-openssl \
  python3-pip \
  python3-venv \
  softhsm2 \
  sqlite3 \
  strace \
  valgrind \
  wget \
  xsltproc \
  zip \
  unzip

WORKDIR /ostree
RUN git init && git remote add origin https://github.com/ostreedev/ostree
RUN git fetch origin v2021.6 && git checkout FETCH_HEAD

WORKDIR /
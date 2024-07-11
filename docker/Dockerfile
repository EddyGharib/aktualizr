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
  clang-format-11 \
  clang-tidy-11 \
  cmake \
  cppcheck \
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
  libjsoncpp-dev \
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
  npm \
  opensc \
  net-tools \
  pkg-config \
  psmisc \
  python3-dev \
  python3-gi \
  python3-openssl \
  python3-pip \
  python3-venv \
  shellcheck \
  softhsm2 \
  sqlite3 \
  strace \
  valgrind \
  wget \
  xsltproc \
  zip \
  unzip

RUN curl -fsSL https://deb.nodesource.com/setup_20.x -o nodesource_setup.sh
RUN bash nodesource_setup.sh
RUN apt-get install -y nodejs

WORKDIR /ostree
RUN git init && git remote add origin https://github.com/ostreedev/ostree
RUN git fetch origin v2021.6 && git checkout FETCH_HEAD
RUN NOCONFIGURE=1 ./autogen.sh
RUN ./configure CFLAGS='-Wno-error=missing-prototypes -Wno-error=aggregate-return' --with-libarchive --disable-gtk-doc --disable-gtk-doc-html --disable-gtk-doc-pdf --disable-man --with-builtin-grub2-mkconfig --with-curl --without-soup --prefix=/usr
RUN make VERBOSE=1 -j4
RUN make install

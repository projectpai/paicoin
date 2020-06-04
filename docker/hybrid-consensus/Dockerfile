
FROM ubuntu:16.04
MAINTAINER Florin Balate <florin.balate@upandrunningsoftware.com>

ARG PAICOIN_BRANCH="hybrid-consensus"

RUN mkdir /paicoin

RUN apt-get update -y -qq && \
    apt-get install -y curl build-essential autoconf libtool pkg-config bsdmainutils checkinstall libevent-dev libssl-dev libzmq5-dev \
      libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-program-options-dev libboost-thread-dev libboost-test-dev \
      git

RUN curl -L http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz -o /tmp/db-4.8.30.NC.tar.gz && \
    tar xfz /tmp/db-4.8.30.NC.tar.gz && cd /db-4.8.30.NC/build_unix/ && \
    ../dist/configure --enable-cxx --disable-shared --with-pic --prefix=/usr/local && \
    make install

RUN  git clone -b ${PAICOIN_BRANCH} --single-branch https://github.com/projectpai/paicoin.git /opt/paicoin && cd /opt/paicoin && git checkout ${PAICOIN_BRANCH}

ENV LD_LIBRARY_PATH /usr/local/lib

WORKDIR /opt/paicoin

RUN ./autogen.sh &&  \
    ./configure --prefix=/usr/local \
    --disable-tests \
    --disable-gui-tests \
    --disable-hardening \
    --disable-bench \
    --with-gui=no && \
    make -j $(lscpu | grep -E '^CPU\(s)' | awk '{print $2}') && \
    make install && \
    mkdir /app && \
    ln -s /opt/paicoin/src/paicoind /app/paicoind && \
    ln -s /opt/paicoin/src/paicoin-cli /app/paicoin-cli

WORKDIR /app

CMD ["/bin/bash", "-c", "paicoind -regtest -txindex -printtoconsole"]
# CMD /bin/bash
FROM ubuntu:16.04
MAINTAINER Constantin Yannuk <constantin.yannuk@upandrunningsoftware.com>

RUN mkdir /paicoin

RUN apt-get update -y -qq && \
    apt-get install -y curl build-essential autoconf libtool pkg-config bsdmainutils checkinstall libevent-dev libssl-dev libzmq5-dev \
      libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-program-options-dev libboost-thread-dev libboost-test-dev

CMD curl -L http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz -o /tmp/db-4.8.30.NC.tar.gz && \
    tar xfz /tmp/db-4.8.30.NC.tar.gz && cd /db-4.8.30.NC/build_unix/ && \
    ../dist/configure --enable-cxx --disable-shared --with-pic --prefix=/usr/local && \
    make install && \
    curl -L --user $GIT_USER:$GIT_PASSWD https://github.com/projectpai/paicoin/tarball/paicoin-initial -o /tmp/paicoin.tgz && \
    mkdir /paicoin-$VERSION && tar xfz /tmp/paicoin.tgz --strip 1 -C /paicoin-$VERSION && cd /paicoin-$VERSION && \
    ./autogen.sh && ./configure --disable-bench --prefix=/usr/local && \
    make && checkinstall -y --maintainer projectpai.com --install=no --nodoc --strip --pkgname paicoin --provides paicoin \
      --exclude /usr/local/bin/test_paicoin,/usr/local/lib,/usr/local/include \
      --pkgversion $VERSION --pkgrelease 1 --pkgsource https://github.com/projectpai/paicoin --pakdir /paicoin 

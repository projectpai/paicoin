FROM ubuntu:16.04
MAINTAINER Constantin Yannuk <constantin.yannuk@upandrunningsoftware.com>

COPY *.deb /tmp/

RUN apt-get update -y -qq && \
    apt-get install -y libc6 libgcc1 libstdc++6 libzmq5 libevent-2.0-5 libevent-pthreads-2.0-5 \
      libssl1.0.0 libboost-system1.58.0 libboost-filesystem1.58.0 libboost-chrono1.58.0 \
      libboost-program-options1.58.0 libboost-thread1.58.0 && \
    dpkg -i /tmp/paicoin*.deb

VOLUME /root/.paicoin

EXPOSE 8566 8567 18566 18567 19566 19567

CMD paicoind -txindex -printtoconsole -listenonion=0

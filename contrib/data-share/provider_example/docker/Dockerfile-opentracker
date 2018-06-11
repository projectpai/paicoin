FROM ubuntu:18.04

RUN apt-get update && apt-get install -y \
  cvs \
  gcc \
  make

RUN mkdir build && cd build && cvs -d :pserver:cvs@cvs.fefe.de:/cvs -z9 co libowfat && cd libowfat && make && cd .. && cvs -d:pserver:anoncvs@cvs.erdgeist.org:/home/cvsroot co opentracker
RUN cat /build/opentracker/Makefile | sed -e 's/-lz//g' > /tmp/Makefile && cp /tmp/Makefile /build/opentracker/Makefile
RUN cd /build/opentracker && make && ln -s /build/opentracker/opentracker /usr/local/bin/opentracker

ADD configs/opentracker.conf /usr/local/etc/opentracker.conf

CMD /build/opentracker/opentracker -f /usr/local/etc/opentracker.conf

FROM alpine:latest

ADD ./torrent_storage /opt/api

RUN apk add --update --no-cache transmission-daemon transmission-cli python3 python3-dev py3-pip libmagic
RUN mkdir /root/.transmission && mkdir -p /home/ubuntu/proj && mkdir -p /var/lib/transmission-daemon/downloads/

RUN pip3 install -r /opt/api/requirements.txt
RUN mkdir -p /home/ubuntu/Downloads

#CMD /usr/bin/python3 /opt/api/manage.py runserver 0.0.0.0:8000

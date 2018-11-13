#!/bin/sh
/usr/bin/transmission-daemon --config-dir  /root/.transmission || exit 1
/usr/bin/python3 /opt/api/manage.py runserver 0.0.0.0:8000 || exit 1

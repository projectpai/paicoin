#!/bin/bash

busybox nslookup paicoin-service | grep $(hostname -d) | grep -v $(hostname) | awk '{print $3}' | while read line || [[ -n $line ]]; do echo -n " -connect="$line; done

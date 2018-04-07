#!/bin/bash

service rsyslog start
cron -f # run in foreground mode to keep container alive

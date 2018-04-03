#!/bin/bash

cron -f # run in foreground mode to keep container alive
service rsyslog start

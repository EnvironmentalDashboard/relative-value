# relative-value

[![Build Status](https://travis-ci.org/EnvironmentalDashboard/relative-value.svg?branch=master)](https://travis-ci.org/EnvironmentalDashboard/relative-value)

Calculates the relative value (i.e. orb color) for all rows in the relative_values table, each of which corresponds to a meter. Compile with `make all` and manually install the cron or run as a docker container with `./run.sh`. Remember to add the database file, `db.h` which should contain

```
#define DB_SERVER ""
#define DB_USER ""
#define DB_PASS ""
#define DB_NAME ""
```

If `DRY_RUN` is `#define`d as 1 in `relative_value.c` the calculation is printed instead of updating relative_values table.

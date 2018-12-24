#define _XOPEN_SOURCE // for strptime
#define _GNU_SOURCE // for strptime
#define TARGET_RECORDS "SELECT DISTINCT grouping, meter_uuid FROM relative_values WHERE grouping != '[]' AND grouping != '' AND grouping IS NOT NULL AND permission IS NOT NULL AND meter_uuid IN (SELECT bos_uuid FROM meters WHERE source = 'buildingos') GROUP BY meter_uuid, grouping ORDER BY AVG(last_updated) ASC"
#define CURRENT_READING1 "SELECT AVG(value) FROM meter_data WHERE meter_id = %d AND resolution = 'live' AND recorded >= %d AND value IS NOT NULL"
#define CURRENT_READING2 "SELECT current FROM meters WHERE id = %d"
#define TYPICAL_DATA1 "SELECT value FROM meter_data WHERE meter_id = %d AND value IS NOT NULL AND resolution = '%s' AND HOUR(FROM_UNIXTIME(recorded)) = HOUR(NOW()) AND DAYOFWEEK(FROM_UNIXTIME(recorded)) IN (%s) ORDER BY recorded DESC LIMIT %d"
#define TYPICAL_DATA2 "SELECT value FROM meter_data WHERE meter_id = %d AND value IS NOT NULL AND recorded > %d AND recorded < %d AND resolution = '%s' AND HOUR(FROM_UNIXTIME(recorded)) = HOUR(NOW()) AND DAYOFWEEK(FROM_UNIXTIME(recorded)) IN (%s) ORDER BY value ASC"
#define ISO8601_FORMAT_EST "%Y-%m-%dT%H:%M:%S-04:00" // EST is -4:00
#define SMALL_CONTAINER 255 // small fixed-size container for arrays
#define MED_CONTAINER 510 // just double SMALL_CONTAINER
#define DRY_RUN 1 // if set to 1, print the calculation instead of updating database

#include <stdio.h>
#include <stdlib.h>

float scale(float pct, int min, int max);
int compare(const void *a, const void *b);
float relative_value(float *typical, float current, int size, int min, int max);


/**
 * Updates the relative_values table
 */
#define _XOPEN_SOURCE // for strptime
#define _GNU_SOURCE // for strptime
#define TARGET_RECORDS "SELECT DISTINCT grouping, meter_uuid FROM relative_values WHERE grouping != '[]' AND grouping != '' AND grouping IS NOT NULL AND permission IS NOT NULL AND meter_uuid IN (SELECT bos_uuid FROM meters) GROUP BY meter_uuid, grouping ORDER BY AVG(last_updated) ASC"
#define CURRENT_READING1 "SELECT AVG(value) FROM meter_data WHERE meter_id = %d AND resolution = 'live' AND recorded >= %d AND value IS NOT NULL"
#define CURRENT_READING2 "SELECT current FROM meters WHERE id = %d"
#define TYPICAL_DATA1 "SELECT value FROM meter_data WHERE meter_id = %d AND value IS NOT NULL AND resolution = '%s' AND HOUR(FROM_UNIXTIME(recorded)) = HOUR(NOW()) AND DAYOFWEEK(FROM_UNIXTIME(recorded)) IN (%s) ORDER BY recorded DESC LIMIT %d"
#define TYPICAL_DATA2 "SELECT value FROM meter_data WHERE meter_id = %d AND value IS NOT NULL AND recorded > %d AND recorded < %d AND resolution = '%s' AND HOUR(FROM_UNIXTIME(recorded)) = HOUR(NOW()) AND DAYOFWEEK(FROM_UNIXTIME(recorded)) IN (%s) ORDER BY value ASC"
#define ISO8601_FORMAT_EST "%Y-%m-%dT%H:%M:%S-04:00" // EST is -4:00
#define SMALL_CONTAINER 255 // small fixed-size container for arrays
#define MED_CONTAINER 510 // just double SMALL_CONTAINER
#define DRY_RUN 1 // if set to 1, print the calculation instead of updating database

#include <stdio.h>
#include <string.h>
#include <mysql.h>
#include <stdlib.h>
#include <time.h>
#include <syslog.h>
#include "lib/cJSON/cJSON.h"
#include "db.h"


/**
 * Scale a percent (value which is 0-100) to a new min and max
 * @param  pct value to scale
 * @param  min new min of range
 * @param  max new max of range
 */
float scale(float pct, int min, int max) {
	return (pct / 100.0) * (max - min) + min;
}

/**
 * comparator for qsort
 */
int compare(const void *a, const void *b) {
  float fa = *(const float*) a;
  float fb = *(const float*) b;
  return (fa > fb) - (fa < fb);
}

/**
 * Produces the relative value for a data set given in an array of 'typical' and a 'current' to
 * compare against
 * float array[] = {62.5, 63.0, 65.0, 66.0, 66.5, 70.0};
 * printf("%.3f\n", relative_value(array, 64.0, 6, 0, 100));
 * exit(1); prints 28.571
 */
float relative_value(float *typical, float current, int size, int min, int max) {
	int i, j, k;
	k = i = 0;
	qsort(typical, size, sizeof(float), compare);
	for (; i < size; ++i) {
		if (typical[i] >= current) {
			j = i;
			// If the typical data contains lots of floats that are the same as current,
			// taking the first occurrence understates the relative value
			// This happens often with water meters that are usually 0 (so the typical has a lot of 0s) and the current reading is also 0
			while (j != (size - 1) && current == typical[++j]) {
				++k; // count how many values are the same 
			}
			break;
		}
	}
	float adjusted_i = i + (k/2); // move the index halfway between the flat-lined data
	float rv = (adjusted_i / (size)) * 100;
	return scale(rv, min, max);
}

/**
 * does the heavy lifting
 * @param conn        db connection
 * @param grouping    json grouping
 * @param uuid        meter id
 * @param day_of_week day index
 * @param t           time_t
 */
void update_meter_rv(MYSQL *conn, char *grouping, char *uuid, int day_of_week, time_t t) {
	MYSQL_RES *res;
	MYSQL_ROW row;
	float typical[SMALL_CONTAINER] = {0};
	char query[MED_CONTAINER];
	char day_sql_str[50]; // goes into TYPICAL_DATA definition query
	sprintf(query, "SELECT id FROM meters WHERE bos_uuid = '%s'", uuid); // need the meter id as well as uuid
	if (mysql_query(conn, query)) {
		syslog(LOG_ERR, "Error retrieving meter id from database: %s", mysql_error(conn));
		return;
	}
	res = mysql_store_result(conn);
	row = mysql_fetch_row(res);
	if (row == NULL) {
		syslog(LOG_ERR, "No meter with uuid: '%s'; Skipping.", uuid);
		return;
	}
	int meter_id = atoi(row[0]);
	mysql_free_result(res);
	cJSON *root = cJSON_Parse(grouping);
	int typicali = 0;
	int this_iteration = 0;
	int try_again = 0;
	for (int i = 0; i < cJSON_GetArraySize(root); i++) {
		cJSON *subitem = cJSON_GetArrayItem(root, i);
		cJSON *days = cJSON_GetObjectItem(subitem, "days");
		int num_days = cJSON_GetArraySize(days);
		int k = 0;
		for (int j = 0; j < num_days; j++) { // build the day_sql_str
			int day_index = cJSON_GetArrayItem(days, j)->valueint;
			day_sql_str[k++] = day_index + '0'; // https://stackoverflow.com/a/2279401/2624391
			if (j != num_days - 1) {
				day_sql_str[k++] = ',';
			} else {
				day_sql_str[k] = '\0';
			}
			if (day_of_week == day_index) {
				this_iteration = 1;
			}
		}
		if (this_iteration) {
			// Either calculate the current value based on the average of the last n minutes or the last non null point
			if (cJSON_HasObjectItem(subitem, "minsAveraged")) {
				int secsAveraged = cJSON_GetObjectItem(subitem, "minsAveraged")->valueint * 60;
				sprintf(query, CURRENT_READING1, meter_id, (int) t-secsAveraged);
				try_again = 1;
			} else {
				sprintf(query, CURRENT_READING2, meter_id);
			}
			if (mysql_query(conn, query)) {
				syslog(LOG_ERR, "Error retrieving current reading current reading from database: %s", mysql_error(conn));
				return;
			}
			res = mysql_store_result(conn);
			row = mysql_fetch_row(res);
			if (row == NULL || row[0] == 0x0) {
				mysql_free_result(res);
				if (try_again) {
					sprintf(query, CURRENT_READING2, meter_id);
					if (mysql_query(conn, query)) {
						syslog(LOG_ERR, "Error retrieving current reading current reading from database: %s", mysql_error(conn));
						return;
					}
					res = mysql_store_result(conn);
					row = mysql_fetch_row(res);
				}
				if (row == NULL || row[0] == 0x0) {
					syslog(LOG_ERR, "Unable to retrieve current reading for meter '%s'; Skipping.", uuid);
					return;
				}
			}
			float current = atof(row[0]);
			mysql_free_result(res);
			if (cJSON_HasObjectItem(subitem, "npoints")) {
				sprintf(query, TYPICAL_DATA1, meter_id, "hour", day_sql_str, cJSON_GetObjectItem(subitem, "npoints")->valueint);
			} else if (cJSON_HasObjectItem(subitem, "start")) {
				/**
				 * TODO: test that groupings with a 'start' parameter works. most (all?) groupings use npoints anyways tho
				 */
				struct tm ltm = {0};
				time_t epoch = 0;
				if (strptime(cJSON_GetObjectItem(subitem, "start")->valuestring, ISO8601_FORMAT_EST, &ltm) != NULL) {
					ltm.tm_isdst = -1; // Is DST on? 1 = yes, 0 = no, -1 = unknown
					epoch = mktime(&ltm);
				} else {
					syslog(LOG_ERR, "Unable to parse given date");
					return;
				}
				sprintf(query, TYPICAL_DATA2, meter_id, (int) epoch, (int) t, "hour", day_sql_str);
			} else {
				syslog(LOG_ERR, "Error parsing relative value configuration");
				return;
			}
			if (mysql_query(conn, query)) {
				syslog(LOG_ERR, "Error retrieving typical data from database: %s", mysql_error(conn));
				return;
			}
			res = mysql_store_result(conn);
			row = mysql_fetch_row(res);
			if (row == NULL) {
				syslog(LOG_ERR, "No typical data for meter '%s'; Skipping.", uuid);
				return;
			}
			while ((row = mysql_fetch_row(res))) {
				typical[typicali++] = atof(row[0]);
			}
			mysql_free_result(res);
			float rv = relative_value(typical, current, typicali, 0, 100);
			if (DRY_RUN == 1) {
				printf("meter: %s\n", uuid);
				printf("current: %.3f\n", current);
				printf("rv: %.3f\n", rv);
				puts("typical data:\n[");
				for (int tmp = 0; tmp < typicali; ++tmp) {
					printf("%.3f, ", typical[tmp]);
				}
				puts("]\n\n\n");
			} else {
				sprintf(query, "UPDATE relative_values SET relative_value = %.3f, last_updated = %d WHERE meter_uuid = '%s' AND grouping = '%s'", rv, (int) t, uuid, grouping);
				if (mysql_query(conn, query)) {
					syslog(LOG_ERR, "Error updating relative value records: %s", mysql_error(conn));
					return;
				}
			}
			break;
		}
	}
}

int main(void) {
	openlog("rv_cron", LOG_PID, LOG_CRON);
	time_t t = time(NULL);
	// add 1 so the day index matches what mysql expects: https://dev.mysql.com/doc/refman/5.5/en/date-and-time-functions.html#function_dayofweek
	int day_of_week = localtime(&t)->tm_wday + 1;
	MYSQL *conn;
	conn = mysql_init(NULL);
	// Connect to database
	if (!mysql_real_connect(conn, DB_SERVER,
	DB_USER, DB_PASS, DB_NAME, 0, NULL, 0)) {
		syslog(LOG_ERR, "Error connecting to database: %s", mysql_error(conn));
		return EXIT_FAILURE;
	}
	MYSQL_RES *res;
	MYSQL_ROW row;
	if (mysql_query(conn, TARGET_RECORDS)) {
		syslog(LOG_ERR, "Error retrieving relative value records: %s", mysql_error(conn));
		return EXIT_FAILURE;
	}
	res = mysql_store_result(conn);
	while ((row = mysql_fetch_row(res))) {
		update_meter_rv(conn, row[0], row[1], day_of_week, t);
	}
	mysql_free_result(res);
	return EXIT_SUCCESS;
}

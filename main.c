/**
 * Updates the relative_values table
 */
#include "relative_value.h"
#include "lib/cJSON/cJSON.h"
#include "db.h"
#include <mysql.h>
#include <string.h>
#include <time.h>

static char t_fmt[26];
static char *db_server;
static char *db_user;
static char *db_pass;
static char *db_name;
unsigned int db_port;

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
		fprintf(stderr, "[%s] Error retrieving meter id from database: %s\n", t_fmt, mysql_error(conn));
		return;
	}
	res = mysql_store_result(conn);
	row = mysql_fetch_row(res);
	if (row == NULL) {
		fprintf(stderr, "[%s] No meter with uuid: '%s'; Skipping.\n", t_fmt, uuid);
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
				fprintf(stderr, "[%s] Error retrieving current reading current reading from database: %s\n", t_fmt, mysql_error(conn));
				return;
			}
			res = mysql_store_result(conn);
			row = mysql_fetch_row(res);
			if (row == NULL || row[0] == 0x0) {
				mysql_free_result(res);
				if (try_again) {
					sprintf(query, CURRENT_READING2, meter_id);
					if (mysql_query(conn, query)) {
						fprintf(stderr, "[%s] Error retrieving current reading current reading from database: %s\n", t_fmt, mysql_error(conn));
						return;
					}
					res = mysql_store_result(conn);
					row = mysql_fetch_row(res);
				}
				if (row == NULL || row[0] == 0x0) {
					fprintf(stderr, "[%s] Unable to retrieve current reading for meter '%s'; Skipping.\n", t_fmt, uuid);
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
				if (strptime(cJSON_GetObjectItem(subitem, "start")->valuestring, ISO8601_FORMAT, &ltm) != NULL) {
					ltm.tm_isdst = -1; // Is DST on? 1 = yes, 0 = no, -1 = unknown
					epoch = mktime(&ltm);
				} else {
					fprintf(stderr, "[%s] Unable to parse given date\n", t_fmt);
					return;
				}
				sprintf(query, TYPICAL_DATA2, meter_id, (int) epoch, (int) t, "hour", day_sql_str);
			} else {
				fprintf(stderr, "[%s] Error parsing relative value configuration\n", t_fmt);
				return;
			}
			if (mysql_query(conn, query)) {
				fprintf(stderr, "[%s] Error retrieving typical data from database: %s\n", t_fmt, mysql_error(conn));
				return;
			}
			res = mysql_store_result(conn);
			row = mysql_fetch_row(res);
			if (row == NULL) {
				fprintf(stderr, "[%s] No typical data for meter '%s'; Skipping.\n", t_fmt, uuid);
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
					fprintf(stderr, "[%s] Failed to update relative value for meter '%s'; Query \"%s\" returned error: %s\n", t_fmt, uuid, query, mysql_error(conn));
					return;
				}
			}
			break;
		}
	}
}

int main(void) {
	db_server = getenv("DB_SERVER");
	db_user = getenv("DB_USER");
	db_pass = getenv("DB_PASS");
	db_name = getenv("DB_NAME");
	db_port = atoi(getenv("DB_PORT"));
	time_t t = time(NULL);
	struct tm *t_info = localtime(&t);
	strftime(t_fmt, 26, "%Y-%m-%d %H:%M:%S", t_info);
	// add 1 so the day index matches what mysql expects: https://dev.mysql.com/doc/refman/5.5/en/date-and-time-functions.html#function_dayofweek
	int day_of_week = t_info->tm_wday + 1;
	MYSQL *conn;
	conn = mysql_init(NULL);
	// Connect to database
	if (!mysql_real_connect(conn, db_server,
	db_user, db_pass, db_name, db_port, NULL, 0)) {
		fprintf(stderr, "[%s] Error connecting to database: %s\n", t_fmt, mysql_error(conn));
		return EXIT_FAILURE;
	}
	MYSQL_RES *res;
	MYSQL_ROW row;
	if (mysql_query(conn, TARGET_RECORDS)) {
		fprintf(stderr, "[%s] Error retrieving relative value records: %s\n", t_fmt, mysql_error(conn));
		return EXIT_FAILURE;
	}
	res = mysql_store_result(conn);
	while ((row = mysql_fetch_row(res))) {
		update_meter_rv(conn, row[0], row[1], day_of_week, t);
	}
	mysql_free_result(res);
	return EXIT_SUCCESS;
}

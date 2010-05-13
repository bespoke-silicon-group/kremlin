/**
 * This file defines functions to write to the binary log file.
 *
 * The format of the log file consists of the following fields repeated for
 * each dynamic region.
 *
 * Field Name                            Type
 * ------------------------------------- --------------------
 * dynamic_region.static_id              long long
 * dynamic_region.dynamic_id             long long
 * dynamic_region.start_time             long long
 * dynamic_region.end_time               long long
 * dynamic_region.critical_path_length   long long
 * dynamic_region.parent.static_id       long long
 * dynamic_region.parent.dynamic_id      long long
 *
 * If a region has no parents, then the static and dynamic ids of its parent's
 * fields will be set to -1 (long long).
 */

#include <stdio.h>
#include "defs.h"
#include "udr.h"

/**
 * Opens the log file.
 *
 * @param file_name The name of the log file to open.
 * @return A file descriptor of the log.
 */
File* log_open(const char* file_name)
{
    File* ret = fopen(file_name, "wb");
	assert(ret != NULL);
	return ret;
}

/**
 * Writes to the log file.
 *
 * @param log                   The file to write to.
 * @param static_id             The static id of the region.
 * @param dynamic_id            The dynamic id of the static region.
 * @param start_time            The start time of the region.
 * @param end_time              The end time of the region.
 * @param critical_path_length  The length of the critical path through the
 *                              region.
 * @param parent_static_id      The static id of the parent of this region.
 *                              Use -1 to indicate that this region has no 
 *                              parent.
 * @param parent_dynamic_id     The dynamic id of the parent of this region.
 *                              Use -1 to indicate that this region has no
 *                              parent.
 */
void log_write(File* log, 
               Int64 static_id, 
               Int64 dynamic_id, 
               Int64 start_time, 
               Int64 end_time,
               Int64 critical_path_length,
               Int64 parent_static_id,
               Int64 parent_dynamic_id,
			   Int64 cnt)
{
	int res = 0;
	assert(log != NULL);
    res += fwrite(&static_id,            sizeof(Int64), 1, log);
    res += fwrite(&dynamic_id,           sizeof(Int64), 1, log);
    res += fwrite(&start_time,           sizeof(Int64), 1, log);
    res += fwrite(&end_time,             sizeof(Int64), 1, log);
    res += fwrite(&critical_path_length, sizeof(Int64), 1, log);
    res += fwrite(&parent_static_id,     sizeof(Int64), 1, log);
    res += fwrite(&parent_dynamic_id,    sizeof(Int64), 1, log);
    res += fwrite(&cnt,    				 sizeof(Int64), 1, log);
	assert(static_id < 10000);
	if (end_time <= start_time) {
		fprintf(stderr, "sregion %d has 0 work\n", static_id);
	}
	if (critical_path_length == 0) {
		fprintf(stderr, "sregion %d has 0 cp length\n", static_id);
	}
	if (start_time < end_time)
		assert(critical_path_length > 0);
//	assert(end_time > start_time);
	//fprintf(stderr, "%d", res);
	assert(res == 8);
}

/**
 * Closes the log file
 *
 * @param log The log file to be closed.
 */
void log_close(File* log)
{
    fclose(log);
}

void writeURegion(File* fp, URegion* region) {
	assert(fp != NULL);
	fwrite(&region->uid, sizeof(Int64), 1, fp);
	fwrite(&region->sid, sizeof(Int64), 1, fp);
	fwrite(&region->work, sizeof(Int64), 1, fp);
	fwrite(&region->cp, sizeof(Int64), 1, fp);
	assert(region->cnt != 0);
	fwrite(&region->cnt, sizeof(Int64), 1, fp);
	//fwrite(&region->pSid, sizeof(Int64), 1, fp);
	fwrite(&region->childrenSize, sizeof(Int64), 1, fp);
	
	ChildInfo* current = region->cHeader;
	while (current != NULL) {
		fwrite(&current->uid, sizeof(Int64), 1, fp);	
		fwrite(&current->cnt, sizeof(Int64), 1, fp);	
		current = current->next;
	}			
}

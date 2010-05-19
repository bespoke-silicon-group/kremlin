#ifndef PYRPROF_LOG_H
#define PYRPROF_LOG_H

File* log_open(const char* file_name);      /* Opens a log file */

void log_write(File* log,                   /* Writes to a log file */
               Int64 static_id, 
               Int64 dynamic_id, 
               Int64 start_time, 
               Int64 end_time,
               Int64 critical_path_length,
               Int64 parent_static_id,
               Int64 parent_dynamic_id);

void log_close(File* log);                  /* Closes a log file */

void writeURegion(File* fp, URegion* region);

#endif /* PYRPROF_LOG_H */

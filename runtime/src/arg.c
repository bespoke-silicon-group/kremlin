#include "arg.h"
// These are the levels on which we are ``reporting'' (i.e. writing info out to the
// .bin file)
Level __kremlin_min_level = 0;
Level __kremlin_max_level = 20;


char * __kremlin_output_filename;
int __kremlin_debug;
int __kremlin_debug_level;
int __kremlin_level_to_log = -1;
UInt __kremlin_max_profiled_level = 21;

Level getMaxProfileLevel() {
	return __kremlin_max_profiled_level;
}

Level getMinReportLevel() {
    return __kremlin_min_level;
}

Level getMaxReportLevel() {
    return __kremlin_max_level;
}

char * argGetOutputFileName() {
	return __kremlin_output_filename;
}

UInt getKremlinDebugLevel() {
	return __kremlin_debug_level;
}

UInt getKremlinDebugFlag() {
	return __kremlin_debug;
}


int parseOptionInt(char* option_str) {
	char *dbg_level_str = strtok(option_str,"= ");
	dbg_level_str = strtok(NULL,"= ");

	if(dbg_level_str) {
		return atoi(dbg_level_str);
	}
	else {
		fprintf(stderr,"ERROR: Couldn't parse int from option (%s)\n",option_str);
	}
}

void createOutputFilename() {
	__kremlin_output_filename[0] = '\0'; // "clear" the old name

	strcat(__kremlin_output_filename,"kremlin-L");
	char level_str[5];
	//sprintf(level_str,"%d",__kremlin_level_to_log);
	sprintf(level_str,"%d",__kremlin_min_level);
	strcat(__kremlin_output_filename,level_str);

	if(__kremlin_max_level != (__kremlin_min_level)) {
		strcat(__kremlin_output_filename,"_");
		level_str[0] - '\0';
		sprintf(level_str,"%d",__kremlin_max_level);
		strcat(__kremlin_output_filename,level_str);
	}
	
	strcat(__kremlin_output_filename,".bin");
}

void parseKremlinOptions(int argc, char* argv[], int* num_args, char*** real_args) {
	int num_true_args = 0;
	char** true_args = malloc(argc*sizeof(char*));

	int i;
	for(i = 0; i < argc; ++i) {
		//fprintf(stderr,"checking %s\n",argv[i]);
		char *str_start;

		str_start = strstr(argv[i],"kremlin-debug");

		if(str_start) {
			__kremlin_debug= 1;
			__kremlin_debug_level = parseOptionInt(argv[i]);

			continue;
		}

		str_start = strstr(argv[i],"kremlin-ltl");
		if(str_start) {
			__kremlin_level_to_log = parseOptionInt(argv[i]);
			__kremlin_min_level = __kremlin_level_to_log;
			__kremlin_max_level = __kremlin_level_to_log;
			__kremlin_max_profiled_level = __kremlin_max_level + 1;

			continue;
		}

		str_start = strstr(argv[i],"kremlin-min-level");
		if(str_start) {
			__kremlin_min_level = parseOptionInt(argv[i]);
			continue;
		}

		str_start = strstr(argv[i],"kremlin-max-level");
		if(str_start) {
			__kremlin_max_level = parseOptionInt(argv[i]);
			__kremlin_max_profiled_level = __kremlin_max_level + 1;
			continue;
		}
		else {
			true_args[num_true_args] = strdup(argv[i]);
			num_true_args++;
		}
	}

	createOutputFilename();

	true_args = realloc(true_args,num_true_args*sizeof(char*));

	*num_args = num_true_args;
	*real_args = true_args;
}

// look for any kremlin specific inputs to the program
int main(int argc, char* argv[]) {
	int num_args = 0;;
	char** real_args;

	__kremlin_output_filename = malloc(20*sizeof(char));
	strcat(__kremlin_output_filename,"kremlin.bin");

	parseKremlinOptions(argc,argv,&num_args,&real_args);

	if(__kremlin_level_to_log == -1) {
    	fprintf(stderr, "[kremlin] min level = %d, max level = %d\n", __kremlin_min_level, __kremlin_max_level);
	}
	else {
    	fprintf(stderr, "[kremlin] logging only level %d\n", __kremlin_level_to_log);
	}

	fprintf(stderr,"[kremlin] writing data to: %s\n", argGetOutputFileName());

	__main(argc,argv);
}
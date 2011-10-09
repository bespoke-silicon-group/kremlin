#include "arg.h"
// These are the levels on which we are ``reporting'' (i.e. writing info out to the
// .bin file)

char * __kremlin_output_filename;
int __kremlin_debug;
int __kremlin_debug_level;
int __kremlin_level_to_log = -1;



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

	strcat(__kremlin_output_filename, "kremlin-L");
	char level_str[5];
	//sprintf(level_str,"%d",__kremlin_level_to_log);
	sprintf(level_str, "%d", getMinLevel());
	strcat(__kremlin_output_filename, level_str);

	if(getMaxLevel() != (getMinLevel())) {
		strcat(__kremlin_output_filename,"_");
		level_str[0] - '\0';
		sprintf(level_str,"%d", getMaxLevel());
		strcat(__kremlin_output_filename,level_str);
	}
	
	strcat(__kremlin_output_filename,".bin");
}

int parseKremlinOptions(int argc, char* argv[], int* num_args, char*** real_args) {
	int num_true_args = 0;
	char** true_args = malloc(argc*sizeof(char*));

	int parsed = 0;
	int i;
	for(i = 0; i < argc-1; ++i) {
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
			setMinLevel(__kremlin_level_to_log);
			setMaxLevel(__kremlin_level_to_log + 1);
			continue;
		}

		str_start = strstr(argv[i],"cache-size");
		if(str_start) {
			setCacheSize(parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"table-type");
		if(str_start) {
			setTableType(parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"kremlin-min-level");
		if(str_start) {
			setMinLevel(parseOptionInt(argv[i]));
			continue;
		}

		str_start = strstr(argv[i],"kremlin-max-level");
		if(str_start) {
			setMaxLevel(parseOptionInt(argv[i])+1);
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

	__kremlin_output_filename = calloc(sizeof(char), 20);
	strcat(__kremlin_output_filename,"kremlin.bin");

	parseKremlinOptions(argc,argv,&num_args,&real_args);

	if(__kremlin_level_to_log == -1) {
    	fprintf(stderr, "[kremlin] min level = %d, max level = %d\n", getMinLevel(), getMaxLevel());
	}
	else {
    	fprintf(stderr, "[kremlin] logging only level %d\n", __kremlin_level_to_log);
	}

	fprintf(stderr,"[kremlin] writing data to: %s\n", argGetOutputFileName());
	//fprintf(stderr,"[kremlin] true args = %d\n", num_args);

	//__main(argc,argv);
	int i;
	char** start = &argv[argc - num_args];
#if 0
	for (i=0; i<num_args; i++) {
		fprintf(stderr, "arg %d: %s\n", i, start[i]);
	}
#endif
	__main(num_args, start);
}

#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>

#include "nfa.h"

#define VERSION "0.1"
#define DPRINTF(ARGS) if(debug==1) printf ARGS;

int debug = 0;

void print_help(char* command_name){
	printf("Usage: %s -f filename -e expression\n", command_name);
}

void print_version(){
	printf("CUDA-sed V" VERSION "\n");
}

int main(int argc, char* argv[]){
	char* shortopts = "hvdf:e:";
	static const struct option longopts[] = {
		{"file", 1, NULL, 'f'},
		{"expression", 1, NULL, 'e'},
		{"debug", 0, NULL, 'd'},
		{"help", 0, NULL, 'h'},
		{"version", 0, NULL, 'v'},
		{NULL, 0, NULL, 0},
	};
	int opt;
	while((opt = getopt_long(argc, argv, shortopts, longopts, NULL))!=EOF){
		switch (opt) {
			case 'f':
				DPRINTF(("Input File: %s\n", optarg));
				break;
			case 'e':
				DPRINTF(("Expression: %s\n", optarg));
				printpost(re2post(optarg));
				break;
			case 'd':
				debug = 1;
				DPRINTF(("Debug set to True\n"));
				break;
			case 'h':
				print_help(argv[0]);
				exit(0);
				break;
			case 'v':
				print_version();
				exit(0);
				break;
			default:
				break;
		}
	}
}

#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "nfa.h"

#define MAX_EXPR 10
#define MAX_LINE_LENGTH 100
#define NUM_BLOCKS 3
#define VERSION "0.1"
#define DPRINTF(ARGS) if(debug==1) printf ARGS;

int debug = 0;

void print_help(char* command_name){
	printf("Usage: %s -f filename -e expression\n", command_name);
}

void print_version(){
	printf("CUDA-sed V" VERSION "\n");
}

NFAset* process_expression(char* expr, char** replacement_strings){
	char* expressions[MAX_EXPR];
	int count = 0;
	expressions[count++] = strtok(expr, ";");
	while(expressions[count-1] != NULL && count<MAX_EXPR){
		expressions[count++] = strtok(NULL, ";");
	}
	count--;
	NFA** nfas = (NFA**)malloc (sizeof(NFA*)*count);
	int* metadata = (int*) malloc (sizeof(int)*3*count);
	char* replacement[count];
	int replacement_total_length = 0;
	for(int expr_id=0;expr_id<count;expr_id++){
		DPRINTF(("Expression %d: %s\n",expr_id, expressions[expr_id]));
		char* command = strtok(expressions[expr_id], "/");
		if(command==NULL || command[0] != 's'){
			DPRINTF(("Only s command is supported\n"));
			exit(0);
		}
		char* search_regex = strtok(NULL, "/");
		replacement[expr_id] = strtok(NULL, "/");
		char* extra = strtok(NULL, "/");
		if(search_regex == NULL || replacement[expr_id] == NULL){
			DPRINTF(("Error in command syntax\n"));
			exit(0);
		}
		if(extra != NULL && index(extra, 'g')!=NULL){
			metadata[expr_id*3 + 0] = 1;
		} else {
			metadata[expr_id*3 + 0] = 0;
		}
		metadata[expr_id*3 + 1] = replacement_total_length;
		replacement_total_length += strlen(replacement[expr_id]);
		metadata[expr_id*3 + 2]= strlen(replacement[expr_id]);
		nfas[expr_id] = post2nfa(re2post(search_regex));
		if(debug == 1){
			char dotfile[10];
			sprintf(dotfile, "nfa%d.dot", expr_id);
			printDot(nfas[expr_id], dotfile);
		}
	}
	*replacement_strings = (char*) malloc (sizeof(char)*replacement_total_length);
	int index = 0;
	for(int expr_id = 0; expr_id<count; expr_id++){
		memcpy(*replacement_strings + index, replacement[expr_id], strlen(replacement[expr_id]));
		index += strlen(replacement[expr_id]);
	}
	return NFA2NFAset(nfas, count, metadata);
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
	char *expression = NULL, *replacement_strings, *filename = NULL;
	while((opt = getopt_long(argc, argv, shortopts, longopts, NULL))!=EOF){
		switch (opt) {
			case 'f':
				DPRINTF(("Input File: %s\n", optarg));
				filename = strdup(optarg);
				break;
			case 'e':
				DPRINTF(("Expression: %s\n", optarg));
				expression = strdup(optarg);
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

	if(expression == NULL || filename == NULL){
		printf("Expression or filename required\n");
		exit(0);
	}
	// Process the expression and convert to NFA
	NFAset* nfaset = process_expression(expression, &replacement_strings);
	if(debug==1){
		printNFAset(nfaset, replacement_strings);
	}
	// Copy NFAset and replacement string to GPU
	// Open the File and split based on newline character
	// Copy lines to GPU
	// Process the lines
	// Copy processed lines back to host
	// Print processed lines
	
	struct stat sb;
	int fd = open(filename, O_RDONLY);
	if(fstat(fd, &sb) == -1){ 
		printf("Error reading filestats\n");
		exit(0);
	}

	char* input_buffer = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if(input_buffer == MAP_FAILED){
		printf("Cannot MMap the input file\n");
		exit(0);
	}

	char* buffer_pointer = input_buffer;
	char* old_buffer_pointer;
	while(buffer_pointer < input_buffer + sb.st_size){
		char *line, *new_line;
		line = (char*) malloc (sizeof(char)*MAX_LINE_LENGTH);
		new_line = (char*) malloc (sizeof(char)*MAX_LINE_LENGTH);

		int line_size = 0;
		old_buffer_pointer = buffer_pointer;
		while(*buffer_pointer != '\n'){
			buffer_pointer++;
			line_size++;
		}
		buffer_pointer++;
		memcpy(line, old_buffer_pointer, sizeof(char)*line_size);

		int current_start;
		//int line_length = strlen(line);
		int line_length = line_size;
		int new_line_length;
		for(int nfa_id = 0; nfa_id < nfaset->n_nfa; nfa_id++){
			current_start = 0;
			new_line_length = 0;
			int* current_states = (int*) malloc (sizeof(int)*MAX_CURRENT_STATES);
			int* next_states = (int*) malloc (sizeof(int)*MAX_CURRENT_STATES);
			int n_current_states, n_next_states;
			int* nfadata = nfaset->nfadata + nfaset->nfadata[nfa_id*5 + 0];
			int is_global = nfaset->nfadata[nfa_id*5 + 2];
			char* replacement_string = replacement_strings + nfaset->nfadata[nfa_id*5 + 3];
			int replacement_length = nfaset->nfadata[nfa_id*5 + 4];
			int match_len, last_final_state;
			do{
				n_current_states = 0;
				n_next_states = 0;
				match_len = 0;
				last_final_state = -1;
				add_to_next_states(nfadata, nfadata[0], current_states, &n_current_states);
				if(check_for_final_state(nfadata[1], current_states, n_current_states) == 1){
					last_final_state = match_len;
				}
				while(n_current_states != 0 && (current_start + match_len < line_length)){
					for(int state_id = 0; state_id < n_current_states; state_id++){
						make_nfa_transition(nfadata, line[current_start + match_len], current_states[state_id], next_states, &n_next_states);
					}
					if(check_for_final_state(nfadata[1], next_states, n_next_states) == 1){
						last_final_state = match_len;
					}
					int* temp = next_states;
					next_states = current_states;
					current_states = temp;
					n_current_states = n_next_states;
					n_next_states = 0;
					match_len++;
				}
				if(last_final_state != -1){
					current_start += last_final_state + 1;
					for(int i = 0; i < replacement_length; i++){
						new_line[new_line_length++] = replacement_string[i];
					}
					if(is_global == 0) {
						while(current_start < line_length){
							new_line[new_line_length++] = line[current_start++];
						}
						break;
					}
				} else if(current_start < line_length) {
					new_line[new_line_length++] = line[current_start];
					current_start++;
				}
			} while (current_start < line_length);
			char* temp = line;
			line = new_line;
			new_line = temp;
			line_length = new_line_length;
			new_line_length = 0;
		}
		for(int i=0; i<line_length; i++){
			printf("%c", line[i]);
		}
		printf("\n");

	}

	munmap(input_buffer, sb.st_size);
}

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
#include "kernels.h"

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
	*(*replacement_strings + index) = '\0';
	return NFA2NFAset(nfas, count, metadata);
}

int main(int argc, char* argv[]){
	char* shortopts = "hvcdf:e:b:";
	static const struct option longopts[] = {
		{"file", 1, NULL, 'f'},
		{"expression", 1, NULL, 'e'},
		{"debug", 0, NULL, 'd'},
		{"help", 0, NULL, 'h'},
		{"version", 0, NULL, 'v'},
		{"cpu", 0, NULL, 'c'},
		{"blocks", 0, NULL, 'b'},
		{NULL, 0, NULL, 0},
	};
	int opt;
	int cpu = 0, blocks = NUM_BLOCKS; 
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
			case 'c':
				cpu = 1;
				break;
			case 'b':
				blocks = atoi(optarg);
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

	struct stat sb;
	int fd = open(filename, O_RDONLY);
	if(fstat(fd, &sb) == -1){ 
		printf("Error reading filestats\n");
		exit(0);
	}

	char* input_buffer = (char*) mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if(input_buffer == MAP_FAILED){
		printf("Cannot MMap the input file\n");
		exit(0);
	}

	char* buffer_pointer = input_buffer;
	char* start_pointer = input_buffer;
	if(cpu==0){
		// Copy NFAset and replacement string to GPU
		int *dnfaset = copyNfasetToGPU(nfaset);
		char *dreplacement_strings = copyReplacementStringsToGPU(replacement_strings);

		int lastnewline = 1, num_lines;
		while(buffer_pointer < input_buffer + sb.st_size){
			// Open the File and split based on newline character
			int *linestart = (int*) malloc (sizeof(int)*blocks*THREADS_PER_BLOCK);
			int *linelen = (int*) malloc (sizeof(int)*blocks*THREADS_PER_BLOCK);
			start_pointer = buffer_pointer;
			int blockdatalen[blocks], oldblockno=0, sumblocklen = 0;
			for(int i=0; i<blocks; i++){
				blockdatalen[i] = 0;
			}
			num_lines = -1;
			for(int line_no=0; line_no<blocks*THREADS_PER_BLOCK; line_no++){
				if(buffer_pointer >= input_buffer + sb.st_size){
					linestart[line_no] = -1;
					linelen[line_no] = 0;
				} else {
					int line_size = 0;
					int block_no = (int)(line_no/THREADS_PER_BLOCK);
					if(oldblockno != block_no){
						sumblocklen+=blockdatalen[oldblockno];
						oldblockno = block_no;
					}
					if(block_no != 0){
						linestart[line_no] = block_no*THREADS_PER_BLOCK*MAX_LINE_LENGTH  + (buffer_pointer - start_pointer) - sumblocklen;
					} else {
						linestart[line_no] = block_no*THREADS_PER_BLOCK*MAX_LINE_LENGTH  + (buffer_pointer - start_pointer);
					}
					while(buffer_pointer < input_buffer + sb.st_size && *buffer_pointer != '\n'){
						buffer_pointer++;
						line_size++;
					}
					if(buffer_pointer < input_buffer + sb.st_size){
						blockdatalen[block_no] += line_size + 1;
					} else {
						blockdatalen[block_no] += line_size;
						lastnewline = 0;
					}
					linelen[line_no] = line_size;
					buffer_pointer++;
					if(buffer_pointer >= input_buffer + sb.st_size){
						num_lines = line_no + 1;
					}
				}
			}
			if(num_lines == -1){
				num_lines = blocks*THREADS_PER_BLOCK;
			}

			// Copy lines to GPU
			int *dlinestart, *dlinelen;
			char *dbuffer;
			copyLinesToGPU(linestart, linelen, start_pointer, blockdatalen, &dlinestart, &dlinelen, &dbuffer, blocks);
			start_pointer = buffer_pointer;
			free(linestart);
			free(linelen);

			// Process the lines
			int size = (nfaset->nfadata[(nfaset->n_nfa-1)*5+0] + nfaset->nfadata[(nfaset->n_nfa-1)*5+1]) * sizeof(int) + ((strlen(replacement_strings) * sizeof(char))/sizeof(int) + 1)*sizeof(int);
			processLines<<<blocks,THREADS_PER_BLOCK, size>>>(dnfaset, nfaset->nfadata[(nfaset->n_nfa-1)*5+0] + nfaset->nfadata[(nfaset->n_nfa-1)*5+1], nfaset->n_nfa, dreplacement_strings,strlen(replacement_strings), dlinestart, dlinelen, dbuffer);
			cudaDeviceSynchronize();

			// Copy processed lines back to host and print the lines
			copyLinesBackAndPrint(dbuffer, num_lines, lastnewline, blocks);

		}

	} else {
		char* old_buffer_pointer;
		while(buffer_pointer < input_buffer + sb.st_size){
			char *line, *new_line;
			line = (char*) malloc (sizeof(char)*MAX_LINE_LENGTH);
			new_line = (char*) malloc (sizeof(char)*MAX_LINE_LENGTH);

			int line_size = 0;
			old_buffer_pointer = buffer_pointer;
			while(buffer_pointer < input_buffer + sb.st_size && *buffer_pointer != '\n'){
				buffer_pointer++;
				line_size++;
			}
			buffer_pointer++;
			memcpy(line, old_buffer_pointer, sizeof(char)*line_size);

			int current_start;
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
	}

	munmap(input_buffer, sb.st_size);
}

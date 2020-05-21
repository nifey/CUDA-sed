#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include "nfa.h"

int* copyNfasetToGPU(NFAset* nfaset){
    int* dnfaset;
    int nfadatalen = nfaset->nfadata[(nfaset->n_nfa-1)*5+0] + nfaset->nfadata[(nfaset->n_nfa-1)*5+1];
    cudaMalloc(&dnfaset, sizeof(int)*nfadatalen);
    cudaMemcpy(dnfaset, nfaset->nfadata, sizeof(int)*nfadatalen, cudaMemcpyHostToDevice);
    return dnfaset;
}

char* copyReplacementStringsToGPU(char* strings){
    char* dstrings;
    cudaMalloc(&dstrings, sizeof(char)*strlen(strings));
    cudaMemcpy(dstrings, strings, sizeof(char)*strlen(strings), cudaMemcpyHostToDevice);
    return dstrings;
}

void copyLinesToGPU(int* linestart,int* linelen, char* input_buffer, int* blockdatalen, int** dlinestart, int** dlinelen, char** dbuffer){
  cudaMalloc(dlinestart, sizeof(int)*NUM_BLOCKS*THREADS_PER_BLOCK);
  cudaMemcpy(*dlinestart, linestart, sizeof(int)*NUM_BLOCKS*THREADS_PER_BLOCK, cudaMemcpyHostToDevice);
  cudaMalloc(dlinelen, sizeof(int)*NUM_BLOCKS*THREADS_PER_BLOCK);
  cudaMemcpy(*dlinelen, linelen, sizeof(int)*NUM_BLOCKS*THREADS_PER_BLOCK, cudaMemcpyHostToDevice);

  cudaMalloc(dbuffer, sizeof(char)*NUM_BLOCKS*THREADS_PER_BLOCK*MAX_LINE_LENGTH);
  char *current = input_buffer;
  for(int i=0; i<NUM_BLOCKS; i++){
    cudaMemcpy(*dbuffer + (i* MAX_LINE_LENGTH * THREADS_PER_BLOCK), current, sizeof(char) * blockdatalen[i], cudaMemcpyHostToDevice);
    current += blockdatalen[i];
  }
}

void copyLinesBackAndPrint(char* dbuffer, int num_lines, int lastnewline){
  char* buffer = (char* ) malloc (sizeof(char)*NUM_BLOCKS*THREADS_PER_BLOCK*MAX_LINE_LENGTH);
  cudaMemcpy(buffer, dbuffer, sizeof(char)*NUM_BLOCKS*THREADS_PER_BLOCK*MAX_LINE_LENGTH, cudaMemcpyDeviceToHost);
	int i;
  for(i=0; i<num_lines-1; i++){
    if(strlen(buffer+i*MAX_LINE_LENGTH) != 0){
      printf("%s\n", buffer+i*MAX_LINE_LENGTH);
    } else {
			printf("\n");
		}
  }
	//The last line might not have a newline character
  if(strlen(buffer+i*MAX_LINE_LENGTH) != 0){
		if(lastnewline == 1){
				printf("%s\n", buffer+i*MAX_LINE_LENGTH);
		} else {
				printf("%s", buffer+i*MAX_LINE_LENGTH);
		}
	} else {
		printf("\n");
  }
  cudaFree(dbuffer);
  free(buffer);
}

__host__ __device__ void add_to_next_states(int* nfadata, int state, int* next_states, int* n_next_states){
	int split_index, split_len = 0;
	int n_char_entry = (nfadata[3] - 2)/3;
	for(int i=0; i<n_char_entry; i++){
		if(nfadata[2+ i*3 + 0] == SPLIT_STATE){
			split_index = nfadata[2+ i*3 + 1];
			split_len = nfadata[2+ i*3 + 2];
		}
	}
	int split_states[2], flag = 0;
	for(int i=0; i<split_len; i++){
		if(nfadata[split_index + i*2 + 0] == state){
			split_states[flag++] = nfadata[split_index + i*2 + 1];
		}
		if(flag==2)
			break;
	}
	if(flag == 0){
		split_states[flag++] = state;
	}
	for(int k = 0; k < flag; k++){
		for(int i = 0; i< *n_next_states; i++){
			if(next_states[i] == split_states[k])
				return;
		}
		next_states[*n_next_states] = split_states[k];
		*n_next_states = *n_next_states + 1;
	}
}

__host__ __device__ void make_nfa_transition(int* nfadata, char character, int state, int* next_states, int* n_next_states){
	int n_char_entry = (nfadata[3] - 2)/3;
	int any_index, any_len = 0;
	for(int i=0; i<n_char_entry; i++){
		if (nfadata[2+ i*3 + 0] == MATCH_ANY){
			any_index = nfadata[2 + i*3 + 1];
			any_len = nfadata[2 + i*3 + 2];
		}
	}
	for(int i=0; i<any_len; i++){
		if(nfadata[any_index + i*2 + 0] == state){
			add_to_next_states(nfadata, nfadata[any_index + i*2 + 1], next_states, n_next_states);
		}
	}
	for(int i=0; i<n_char_entry; i++){
		if(nfadata[2+ i*3 + 0] == (int) character){
			for(int j=0; j<nfadata[2+i*3 + 2]; j++){
				if(nfadata[nfadata[2 + i*3 + 1] + j*2 + 0] == state){
					add_to_next_states(nfadata, nfadata[nfadata[2 + i*3 + 1] + j*2 + 1], next_states, n_next_states);
				}
			}
			break;
		}
	}
}

__global__ void processLines(int* nfaset,int n_nfa, char* replacement_strings,int* dlinestart,int* dlinelen,char* dbuffer){
    int id = blockIdx.x * blockDim.x + threadIdx.x;
    char *line, *new_line;
    line = (char*) malloc (sizeof(char)*MAX_LINE_LENGTH);
    new_line = (char*) malloc (sizeof(char)*MAX_LINE_LENGTH);
    int *current_states, *next_states, n_current_states, n_next_states;
    current_states = (int*) malloc (sizeof(int)*MAX_CURRENT_STATES);
    next_states = (int*)  malloc (sizeof(int)*MAX_CURRENT_STATES);
  	int current_start;
    int line_length = dlinelen[id];
		int new_line_length;
    for(int i=0; i<line_length; i++){
      line[i] = dbuffer[dlinestart[id] + i];
    }
		for(int nfa_id = 0; nfa_id < n_nfa; nfa_id++){
			current_start = 0;
			new_line_length = 0;
			int* nfadata = nfaset + nfaset[nfa_id*5 + 0];
			int is_global = nfaset[nfa_id*5 + 2];
			char* replacement_string = replacement_strings + nfaset[nfa_id*5 + 3];
			int replacement_length = nfaset[nfa_id*5 + 4];
			int match_len, last_final_state;
			do{
				n_current_states = 0;
				n_next_states = 0;
				match_len = 0;
				last_final_state = -1;
				add_to_next_states(nfadata, nfadata[0], current_states, &n_current_states);
        for(int i=0; i<n_current_states; i++){
          if(current_states[i] == nfadata[1]){
              last_final_state = match_len;
              break;
          }
        }
				while(n_current_states != 0 && (current_start + match_len < line_length)){
					for(int state_id = 0; state_id < n_current_states; state_id++){
						make_nfa_transition(nfadata, line[current_start + match_len], current_states[state_id], next_states, &n_next_states);
					}
          for(int i=0; i<n_next_states; i++){
            if(next_states[i] == nfadata[1]){
              last_final_state = match_len;
              break;
            }
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
    __syncthreads();
    int i;
		for(i=0; i<line_length; i++){
      dbuffer[MAX_LINE_LENGTH*id + i] = line[i];
		}
    dbuffer[MAX_LINE_LENGTH*id + i] = '\0';
    free(line);
    free(new_line);
    free(current_states);
    free(next_states);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nfa.h"

atom* createAtom(int special, char character){
	atom* a = (atom*) malloc (sizeof(atom));
	a->special = special;
	a->character = character;
	return a;
}

int appendBuffer(atom** buffer, int* bufferindex, int special, char character){
	if((*bufferindex)>=MAX_RE_LENGTH) return 0;
	buffer[(*bufferindex)] = createAtom(special, character);
	(*bufferindex)++;
	return 1;
}

atom** re2post(char* re){
	atom** buffer = (atom**) malloc (sizeof(atom*) * MAX_RE_LENGTH);
	int escaped = 0, bufferindex=0;
	struct {
		int natom, nalt;
	} parens[MAX_PARENS], *p;
	p = parens;
	int natom=0, nalt=0;
	for(;*re;re++){
		char c = *re;
		switch (c) {
			case '(':
				if(escaped){
					if(natom > 1){
						natom--;
						if(!appendBuffer(buffer, &bufferindex, 1, '.'))
							return NULL;
					}
					if(p >= parens+MAX_PARENS)
						return NULL;
					p->natom = natom;
					p->nalt = nalt;
					natom = 0;
					nalt = 0;
					p++;
					escaped=0;
				} else {
					if(natom > 1){
						--natom;
						if(!appendBuffer(buffer, &bufferindex, 1, '.'))
							return NULL;
					}
					if(!appendBuffer(buffer, &bufferindex, 0, c))
						return NULL;
					natom++;
				}
				break;
			case ')':
				if(escaped){
					if(p == parens || natom == 0)
						return NULL;
					while(--natom > 0){
						if(!appendBuffer(buffer, &bufferindex, 1, '.'))
							return NULL;
					}
					while(nalt-- > 0){
						if(!appendBuffer(buffer, &bufferindex, 1, '|'))
							return NULL;
					}
					--p;
					nalt = p->nalt;
					natom = p->natom;
					natom++;
					escaped=0;
				} else {
					if(natom > 1){
						--natom;
						if(!appendBuffer(buffer, &bufferindex, 1, '.'))
							return NULL;
					}
					if(!appendBuffer(buffer, &bufferindex, 0, c))
						return NULL;
					natom++;
				}
				break;
			case '\\':
				if(escaped){
					if(natom > 1){
						--natom;
						if(!appendBuffer(buffer, &bufferindex, 1, '.'))
							return NULL;
					}
					if(!appendBuffer(buffer, &bufferindex, 0, c))
						return NULL;
					natom++;
					escaped=0;
				} else {
					escaped=1;
				}
				break;
			case '|':
				if(escaped){
					if(natom == 0)
						return NULL;
					while(--natom > 0){
						if(!appendBuffer(buffer, &bufferindex, 1, '.'))
							return NULL;
					}
					nalt++;
					escaped=0;
				} else {
					if(natom > 1){
						--natom;
						if(!appendBuffer(buffer, &bufferindex, 1, '.'))
							return NULL;
					}
					if(!appendBuffer(buffer, &bufferindex, 0, c))
						return NULL;
					natom++;
				}
				break;
			case '*':
				if(escaped){
					if(natom > 1){
						--natom;
						if(!appendBuffer(buffer, &bufferindex, 1, '.'))
							return NULL;
					}
					if(!appendBuffer(buffer, &bufferindex, 0, c))
						return NULL;
					natom++;
					escaped=0;
				} else {
					if(natom == 0)
						return NULL;
					if(!appendBuffer(buffer, &bufferindex, 1, c))
						return NULL;
				}
				break;
			case '+':
			case '?':
				if(escaped){
					if(natom == 0)
						return NULL;
					if(!appendBuffer(buffer, &bufferindex, 1, c))
						return NULL;
					escaped=0;
				} else {
					if(natom > 1){
						--natom;
						if(!appendBuffer(buffer, &bufferindex, 1, '.'))
							return NULL;
					}
					if(!appendBuffer(buffer, &bufferindex, 0, c))
						return NULL;
					natom++;
				}
				break;
			case '.':
				if(escaped){
					if(natom > 1){
						--natom;
						if(!appendBuffer(buffer, &bufferindex, 1, '.'))
							return NULL;
					}
					if(!appendBuffer(buffer, &bufferindex, 0, c))
						return NULL;
					natom++;
					escaped=0;
				} else {
					if(!appendBuffer(buffer, &bufferindex, 1, '#')) // Since special . is used for concatenation, special # will denote ANY character
						return NULL;
					natom++;
				}
				break;
			default:
				if(escaped){
					escaped=0;
					continue;
				}else{
					if(natom > 1){
						--natom;
						if(!appendBuffer(buffer, &bufferindex, 1, '.'))
							return NULL;
					}
					if(natom > 1){
						--natom;
						if(!appendBuffer(buffer, &bufferindex, 1, '.'))
							return NULL;
					}
					if(!appendBuffer(buffer, &bufferindex, 0, c))
						return NULL;
					natom++;
				}
		}
	}
	if(p != parens)
		return NULL;
	while(--natom > 0){
		if(!appendBuffer(buffer, &bufferindex, 1, '.'))
			return NULL;
	}
	while(nalt-- > 0){
		if(!appendBuffer(buffer, &bufferindex, 1, '|'))
			return NULL;
	}
	buffer[bufferindex] = NULL;
	return buffer;
}

void printpost(atom** buffer){
	if(buffer == NULL){
		printf("Conversion of RE to Post failed\n");
		return;
	}
	atom** a = buffer;
	atom* current;
	
	while(*a != NULL){
		current = *a;
		if(current->special){
			printf("Special\t");
		} else {
			printf("Character\t");
		}
		printf("%c\n",current->character);
		a++;
	}
}

void printDot(NFA* fragment, char* filename){
	FILE* fp = fopen(filename, "w");
	if( fp == NULL ) {
		printf("Cannot open file %s for printing dot output\n", filename);
		return;
	}
	fprintf(fp, "strict digraph NFA {\n");
	fprintf(fp, "\tstart -> %d;\n", fragment->start);
	fprintf(fp, "\t%d -> end;\n", fragment->end);
	for(int i = 0; i<fragment->n_transitions; i++){
		if(fragment->transitions[3*i + 0] < 256){
			fprintf(fp, "\t%d -> %d [label=%c];\n" ,fragment->transitions[3*i + 1],fragment->transitions[3*i + 2],(char)fragment->transitions[3*i + 0]);
		} else if(fragment->transitions[3*i+0] == MATCH_ANY) {
			fprintf(fp, "\t%d -> %d [label=Any];\n" ,fragment->transitions[3*i + 1],fragment->transitions[3*i + 2]);
		} else {
			fprintf(fp, "\t%d -> %d;\n" ,fragment->transitions[3*i + 1],fragment->transitions[3*i + 2]);
		}
	}
	fprintf(fp, "}\n");
	fclose(fp);
}

void printNFAset(NFAset* nfaset, char* replacement_strings){
	printf("NFAset n_nfa: %d\n", nfaset->n_nfa);
	for(int i=0; i<nfaset->n_nfa; i++){
		printf("NFA %d\n", i);
		printf("Is global replacement: %d\n", nfaset->nfadata[i*5+2]);
		printf("Replacement string: ");
		for(int j=nfaset->nfadata[i*5+3]; j<(nfaset->nfadata[i*5+3] + nfaset->nfadata[i*5+4]); j++){
			printf("%c", replacement_strings[j]);
		}
		printf("\nNFA data: ");
		for(int j=nfaset->nfadata[i*5+0]; j<(nfaset->nfadata[i*5+0] + nfaset->nfadata[i*5+1]); j++){
			printf("%d ", nfaset->nfadata[j]);
		}
		printf("\n\n");
	}
}

NFA* new_fragment(int c){
	NFA* tempnfa = (NFA*) malloc (sizeof(NFA));
	tempnfa->transitions = (int*) malloc (sizeof(int)*(3*MAX_TRANSITIONS));
	tempnfa->transitions[0] = c;
	tempnfa->transitions[1] = 1;
	tempnfa->transitions[2] = 2;
	tempnfa->n_transitions = 1;
	tempnfa->n_states = 2;
	tempnfa->start = 1;
	tempnfa->end = 2;
	return tempnfa;
}

NFA* merge_fragments(NFA* frag1, NFA* frag2, char ch){
	int k, diff;
	switch(ch){
		case '.':
			k = frag1->n_transitions;
			diff = frag1->n_states - 1;
			for(int i = 0; i < frag2->n_transitions; i++){
				frag1->transitions[k*3 + 0] = frag2->transitions[i*3 + 0];
				frag1->transitions[k*3 + 1] = frag2->transitions[i*3 + 1] + diff;
				frag1->transitions[k*3 + 2] = frag2->transitions[i*3 + 2] + diff;
				if(frag2->transitions[i*3 + 1] == frag2->start){
					frag1->transitions[k*3 + 1] = frag1->end;
				}
				if(frag2->transitions[i*3 + 2] == frag2->start){
					frag1->transitions[k*3 + 2] = frag1->end;
				}
				k++;
			}
			frag1->n_transitions = k;
			frag1->n_states += (frag2->n_states - 1);
			frag1->end = frag2->end + diff;
			free(frag2->transitions);
			free(frag2);
			break;
		case '|':
			k = frag1->n_transitions;
			diff = 1;
			for(int i = 0; i < frag1->n_transitions; i++){
				frag1->transitions[i*3 + 1] += diff;
				frag1->transitions[i*3 + 2] += diff;
			}
			diff = frag1->n_states + 1;
			for(int i = 0; i < frag2->n_transitions; i++){
				frag1->transitions[k*3 + 0] = frag2->transitions[i*3 + 0];
				frag1->transitions[k*3 + 1] = frag2->transitions[i*3 + 1] + diff;
				frag1->transitions[k*3 + 2] = frag2->transitions[i*3 + 2] + diff;
				if(frag2->transitions[i*3 + 1] == frag2->end){
					frag1->transitions[k*3 + 1] = frag1->end + 1;
				}
				if(frag2->transitions[i*3 + 2] == frag2->end){
					frag1->transitions[k*3 + 2] = frag1->end + 1;
				}
				k++;
			}
			frag1->transitions[k*3+0] = SPLIT_STATE;
			frag1->transitions[k*3+1] = 1;
			frag1->transitions[k*3+2] = frag1->start + 1;
			k++;
			frag1->transitions[k*3+0] = SPLIT_STATE;
			frag1->transitions[k*3+1] = 1;
			frag1->transitions[k*3+2] = frag2->start + diff;
			k++;
			frag1->start = 1;
			frag1->n_states += (frag2->n_states + 1);
			frag1->end = frag1->end + 1;
			frag1->n_transitions += (frag2->n_transitions + 2);
			free(frag2->transitions);
			free(frag2);
			break;
		case '+':
			diff = 1;
			k = frag1->n_transitions;
			frag1->transitions[k*3+0] = SPLIT_STATE;
			frag1->transitions[k*3+1] = frag1->end;
			frag1->transitions[k*3+2] = frag1->start;
			k++;
			frag1->transitions[k*3+0] = SPLIT_STATE;
			frag1->transitions[k*3+1] = frag1->end;
			frag1->transitions[k*3+2] = frag1->end + diff;
			k++;
			frag1->end = frag1->end + diff;
			frag1->n_transitions += 2;
			frag1->n_states += 1;
			break;
		case '*':
			diff = 1;
			for(int i = 0; i < frag1->n_transitions; i++){
				frag1->transitions[i*3 + 1] += diff;
				frag1->transitions[i*3 + 2] += diff;
				if(frag1->transitions[i*3 + 1] == frag1->end + diff){
					frag1->transitions[i*3 + 1] = 1;
				}
				if(frag1->transitions[i*3 + 2] == frag1->end + diff){
					frag1->transitions[i*3 + 2] = 1;
				}
			}
			k = frag1->n_transitions;
			frag1->transitions[k*3+0] = SPLIT_STATE;
			frag1->transitions[k*3+1] = 1;
			frag1->transitions[k*3+2] = frag1->start + diff;
			k++;
			frag1->transitions[k*3+0] = SPLIT_STATE;
			frag1->transitions[k*3+1] = 1;
			frag1->transitions[k*3+2] = frag1->end + diff;
			k++;
			frag1->n_transitions += 2;
			frag1->start = 1;
			frag1->end += 1;
			frag1->n_states++;
			break;
		case '?':
			diff = 1;
			for(int i=0; i < frag1->n_transitions; i++){
				frag1->transitions[i*3 + 1] += diff;
				frag1->transitions[i*3 + 2] += diff;
			}
			k = frag1->n_transitions;
			frag1->transitions[k*3+0] = SPLIT_STATE;
			frag1->transitions[k*3+1] = 1;
			frag1->transitions[k*3+2] = frag1->start + diff;
			k++;
			frag1->transitions[k*3+0] = SPLIT_STATE;
			frag1->transitions[k*3+1] = 1;
			frag1->transitions[k*3+2] = frag1->end + diff;
			k++;
			frag1->start = 1;
			frag1->end += diff;
			frag1->n_transitions += 2;
			frag1->n_states += 1;

			break;
	}
	return frag1;
}

int sort_transitions(int* transitions, int n_transitions){
	int i, j, data[3];
	if(n_transitions == 1) return 1;
	for(i=1; i<n_transitions; i++){
		j = i - 1;
		data[0] = transitions[i*3 + 0];
		data[1] = transitions[i*3 + 1];
		data[2] = transitions[i*3 + 2];
		while(j>=0 &&
				(data[0] < transitions[j*3 + 0]  ||
				(data[0] == transitions[j*3 + 0] && data[1] < transitions[j*3 + 1]))
				){
			transitions[(j+1)*3 + 0] = transitions[j*3 + 0];
			transitions[(j+1)*3 + 1] = transitions[j*3 + 1];
			transitions[(j+1)*3 + 2] = transitions[j*3 + 2];
			j--;
		}
		transitions[(j+1)*3 + 0] = data[0];
		transitions[(j+1)*3 + 1] = data[1];
		transitions[(j+1)*3 + 2] = data[2];
	}
	int last_seen = transitions[0];
	int n_characters = 1;
	for(i=1; i<n_transitions; i++){
		if(transitions[i*3+0] != last_seen){
			last_seen = transitions[i*3+0];
			n_characters++;
		}
	}
	return n_characters;
}

NFAset* NFA2NFAset(NFA** fragments, int n_nfa, int* metadata){
	NFAset* nfaset = (NFAset*) malloc (sizeof(NFAset));
	nfaset->n_nfa = n_nfa;
	int total_length = 0;
	int **nfadata = (int**) malloc(sizeof(int*)*n_nfa);
	for(int nfa_id=0; nfa_id<n_nfa; nfa_id++){
		NFA* current_nfa = fragments[nfa_id];
		int n_characters = sort_transitions(current_nfa->transitions, current_nfa->n_transitions);
		int length = 2 + 3*(n_characters) + 2*(current_nfa->n_transitions);
		total_length += length;
		nfadata[nfa_id] = (int*) malloc (sizeof(int)*(length+1));
		nfadata[nfa_id][0] = length;
		nfadata[nfa_id][1] = current_nfa->start;
		nfadata[nfa_id][2] = current_nfa->end;
		int char_index, tran_index, last_seen;
		char_index = 3;
		tran_index = 3 + 3*(n_characters);
		nfadata[nfa_id][char_index + 0] = current_nfa->transitions[0];
		nfadata[nfa_id][char_index + 1] = tran_index - 1;
		nfadata[nfa_id][char_index + 2] = 1;
		last_seen = current_nfa->transitions[0];
		nfadata[nfa_id][tran_index + 0] = current_nfa->transitions[1];
		nfadata[nfa_id][tran_index + 1] = current_nfa->transitions[2];
		tran_index += 2;
		for(int i=1;i<current_nfa->n_transitions;i++){
			if(last_seen != current_nfa->transitions[i*3 + 0]){
				char_index += 3;
				nfadata[nfa_id][char_index + 0] = current_nfa->transitions[i*3 + 0];
				nfadata[nfa_id][char_index + 1] = tran_index - 1;
				nfadata[nfa_id][char_index + 2] = 1;
				last_seen = current_nfa->transitions[i*3 + 0];
				nfadata[nfa_id][tran_index + 0] = current_nfa->transitions[i*3 + 1];
				nfadata[nfa_id][tran_index + 1] = current_nfa->transitions[i*3 + 2];
				tran_index += 2;
			} else {
				nfadata[nfa_id][char_index + 2]++;
				nfadata[nfa_id][tran_index + 0] = current_nfa->transitions[i*3 + 1];
				nfadata[nfa_id][tran_index + 1] = current_nfa->transitions[i*3 + 2];
				tran_index += 2;
			}
		}
	}
	nfaset->nfadata = (int*) malloc (sizeof(int)*(total_length + 5*n_nfa));
	int curr_index = 5 * n_nfa;
	for(int nfa_id=0; nfa_id<n_nfa; nfa_id++){
		nfaset->nfadata[nfa_id*5 + 0] = curr_index;
		nfaset->nfadata[nfa_id*5 + 1] = nfadata[nfa_id][0];
		nfaset->nfadata[nfa_id*5 + 2] = metadata[nfa_id*3 + 0];
		nfaset->nfadata[nfa_id*5 + 3] = metadata[nfa_id*3 + 1];
		nfaset->nfadata[nfa_id*5 + 4] = metadata[nfa_id*3 + 2];
		memcpy(nfaset->nfadata + curr_index, nfadata[nfa_id]+1, sizeof(int)*nfadata[nfa_id][0]);
		curr_index += nfadata[nfa_id][0];
		free(nfadata[nfa_id]);
	}
	return nfaset;
}

NFA* post2nfa(atom** buffer){
	atom** atomptr = buffer;
	atom* current;
	NFA **stack, **top;
	stack = (NFA**) malloc (sizeof(NFA*) * 1000);
	top = stack;

	while(*atomptr != NULL){
		current = *atomptr;
		if(current->special){
			NFA *frag1, *frag2;
			switch(current->character){
				case '.':
				case '|':
					top--;
					frag1 = *top;
					top--;
					frag2 = *top;
					*top = merge_fragments(frag2, frag1, current->character);
					top++;
					break;
				case '*':
				case '+':
				case '?':
					top--;
					frag1 = *top;
					*top = merge_fragments(frag1, NULL, current->character);
					top++;
					break;
				case '#':
					*top = new_fragment(MATCH_ANY);
					top++;
			}
		} else {
			*top = new_fragment(current->character);
			top++;
		}
		atomptr++;
	}

	top--;
	if(top != stack){
		printf("Error converting to NFA\n");
		return NULL;
	}
	printDot(*top, "FinalNFA.dot");
	return *top;
}

void add_to_next_states(int* nfadata, int state, int* next_states, int* n_next_states){
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

void make_nfa_transition(int* nfadata, char character, int state, int* next_states, int* n_next_states){
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

int check_for_final_state(int final_state, int* next_states, int n_next_states){
	for(int i=0; i<n_next_states; i++){
		if(next_states[i] == final_state){
			return 1;
		}
	}
	return 0;
}

#include <stdio.h>
#include <stdlib.h>

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

void printDot(TempNFA* fragment, char* filename){
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

TempNFA* new_fragment(int c){
	TempNFA* tempnfa = (TempNFA*) malloc (sizeof(NFA));
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

TempNFA* merge_fragments(TempNFA* frag1, TempNFA* frag2, char ch){
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

NFA* compressTempNFA2NFA(TempNFA* fragment){
	NFA* nfa = (NFA*) malloc (sizeof(NFA));
	nfa->character_table = (int*) malloc (sizeof(int)*(258+2*MAX_TRANSITIONS));
	nfa->transition_table = nfa->character_table + 258;
	for(int i=0;i<258;i++){
		nfa->character_table[i] = -1;
	}
	//TODO Finish this function
}

NFA* post2nfa(atom** buffer){
	atom** atomptr = buffer;
	atom* current;
	TempNFA **stack, **top;
	stack = (TempNFA**) malloc (sizeof(TempNFA*) * 1000);
	top = stack;

	while(*atomptr != NULL){
		current = *atomptr;
		if(current->special){
			TempNFA *frag1, *frag2;
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
	return compressTempNFA2NFA(*top);
}

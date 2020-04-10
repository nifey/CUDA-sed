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
				} else {
					if(natom > 1){
						--natom;
						if(!appendBuffer(buffer, &bufferindex, 1, '.'))
							return NULL;
					}
					if(!appendBuffer(buffer, &bufferindex, 0, c))
						return NULL;
					natom++;
					escaped=0;
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

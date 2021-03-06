#define MAX_TRANSITIONS 1000
#define MAX_RE_LENGTH 1000
#define MAX_PARENS 100
#define MAX_CURRENT_STATES 50

#define MAX_EXPR 10
#define MAX_LINE_LENGTH 150
#define NUM_BLOCKS 2
#define THREADS_PER_BLOCK 1024

#define MATCH_ANY 256
#define SPLIT_STATE 257

typedef struct {
	int special;
	char character;
} atom;

typedef struct {
	int start;
	int end;
	int* transitions;
	int n_transitions;
	int n_states;
} NFA;

typedef struct {
	int n_nfa;
	int* nfadata;
} NFAset;

atom** re2post(char*);
NFA* post2nfa(atom**);
NFAset* NFA2NFAset(NFA**, int, int*);
void printpost(atom**);
void printNFAset(NFAset*, char*);
void printDot(NFA*, char*);
__host__ __device__ void make_nfa_transition(int*, char, int, int*, int*);
int check_for_final_state(int, int*, int);
__host__ __device__ void add_to_next_states(int*, int, int*, int*);

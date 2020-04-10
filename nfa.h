#define MAX_EDGES_IN_NFA 1000
#define MAX_RE_LENGTH 1000
#define MAX_PARENS 100

typedef struct {
	int* character_table;
	int* transition_table;
	int n_transitions;
	int n_states;
} NFA;

typedef struct {
	int special;
	char character;
} atom;

atom** re2post(char*);
NFA* post2nfa(atom**);
void printpost(atom**);

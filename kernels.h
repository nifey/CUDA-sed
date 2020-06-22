int* copyNfasetToGPU(NFAset*);
char* copyReplacementStringsToGPU(char*);
void copyLinesToGPU(int*, int*, char*, int*, int**, int**, char**, int);
__global__ void processLines(int* ,int, int ,char*, int, int* ,int* , char* );
void copyLinesBackAndPrint(char* ,int , int, int);

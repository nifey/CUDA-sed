int* copyNfasetToGPU(NFAset*);
char* copyReplacementStringsToGPU(char*);
void copyLinesToGPU(int*, int*, char*, int*, int**, int**, char**);
__global__ void processLines(int* ,int ,char* ,int* ,int* , char* );
void copyLinesBackAndPrint(char* ,int , int);

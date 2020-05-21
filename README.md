# CUDA Sed

This is a basic implementation of Sed that use the CUDA to perform regular expression matching and substituition on the GPU.

The codes uses a NFA (Non deterministic Finite Automata) to do regular expression matching on the GPU and takes a lot of inspiration from [Russ Cox's writeup on Regular expression matching](https://swtch.com/~rsc/regexp/regexp1.html).

The current version:
- Supports only the 's' (substituite) command
- Supports global substituition using the g flag
- Does not support inline file substituition ( -i commandline flag )
- Only supports a basic regular expressions

The regular expression features that are not supported are:
- Backreferences
- Case insensitive matching with i flag
- Matching the number of instances using curly braces
- ^ and $ characters to denote start and end of string
- Matching using a choice list eg: [abc]

## Compilation

To compile run:
```bash
nvcc main.cu kernels.cu nfa.cu -o sed
./sed -e 's/hello/world/g' -f filename
```

#include"wcc.h"

Token *curtoken;
char *user_input;

FILE *errout;
FILE *quadout;
FILE *codeout;


QuadSet *quadset;


int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "%s: invalid number of arguments\n", argv[0]);
        return 1;
    }
    errout = stderr;
    quadout =  fopen("quad.txt", "wb+");
    codeout = fopen("code.S", "wb+");
    setbuf(codeout, NULL);
    user_input = argv[1];
    Token *tokenList = tokenize();
    curtoken = tokenList;
    Function * funclist = program();
    quadset = gen_quadsets(funclist);
    gen_code();
    // printf(".intel_syntax noprefix\n");
    // printf(".global main\n");
    // printf("main:\n");
    // printf("  mov rax, %d\n", atoi(argv[1]));
    // printf("  ret\n");
    return 0;
}

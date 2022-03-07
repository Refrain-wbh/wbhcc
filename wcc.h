#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define DEBUG

typedef enum
{
    TK_RESERVED, // punctuators or key words
    TK_NUM,   // Integer literals
    TK_EOF,   // End-of-file markers
} TokenKind;

// Token type
typedef struct Token Token;
struct Token  
{
    TokenKind kind; // Token kind
    Token *next;    // Next token
    int val;       // If kind is TK_NUM, its value
    char *str;      //token对应的字符串
    int strlen;     //限制str的长度
};


//****************prase******************//

//AST node type
typedef enum
{
    NK_ADD,     //  +
    NK_SUB,     //  -
    NK_NUM,     //  integer
    NK_DIV,     //  /
    NK_MUL,     //  *
    NK_EQ,      // ==
    NK_NE,      // !=
    NK_LT,      // <  ### > ==> <
    NK_LE,      // <=
    NK_RETURN,  // "return"

} NodeKind;
/*************sym table*****************/ 
typedef struct Temp Temp;
struct Temp
{
    int no;
    //for codegen use
    int reg;//means a reg(index from 1),0 means not in a reg
    int inmemory;//0 means not in memory ,others mean in memory
};
typedef struct TempList TempList;
struct TempList
{
    TempList *next;
    Temp temp;
};

typedef struct Constval Constval;
struct Constval
{
    int val;
    // for codegen use
    int reg;//means a reg(index from 1),0 means not in a reg
};
typedef struct ConstvalList ConstvalList;
struct ConstvalList
{
    ConstvalList *next;
    Constval constval;
};

// AST node
typedef struct Node Node;
struct Node
{
    NodeKind kind;

    Node *lhs;      //  left head side
    Node *rhs;      //  right head side
    Node *next;    //next state
    Constval * constval;       //used if kind == ND_NUM
    Temp *temp;     //used if kind is a operator

};



// quad node
typedef enum
{
    QK_ADD,
    QK_SUB,
    QK_DIV,
    QK_MUL,
    QK_EQ,      // ==
    QK_NE,      // !=
    QK_LT,      // <  ### > ==> <
    QK_LE,      // <=
    QK_RETURN,  // "return"
} QuadKind;
typedef struct Quad Quad;
struct Quad
{
    QuadKind op;
    Node *arg1;
    Node *arg2;
    Node *result;
};
//mid code set
typedef struct QuadSet QuadSet;
struct QuadSet
{
    Quad *list;
    int capacity;
    int size;
};


//gobal var

extern Token *curtoken;
extern char *user_input;

extern FILE *errout;
extern FILE *quadout;
extern FILE *codeout;


extern Node *ASTroot;
extern QuadSet *quadset;



// funtion of tokenize
bool at_eof();
bool consume(char* op);
int expect_num();
void expect(char *op);
Token *tokenize();

//function of symtable
Temp *new_temp();
Constval *new_const();
void print_tempaddr(FILE *out, Temp *temp);

//funtion of parse
Node *program();



//funtion of quadgen
void gen_quadset(Node *ASTroot);
void print_quadset();

//funtion of codegen
void gen_code();

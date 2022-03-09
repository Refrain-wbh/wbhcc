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
    TK_IDENT,   //identity
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
    NK_IDENT,   // id
    NK_ASSIGN,  // =
    NK_IF,      //if
    NK_WHILE,   //while
    NK_FOR,     //for
    NK_BLOCK,   //block
    NK_FUNCTION,//funtion  

} NodeKind;
/*************sym table*****************/

typedef struct Var Var;
struct Var
{
    Var *next;
    char * name;
    int offset;
    int reg;
    int inmemory;
};
typedef struct Temp Temp;
struct Temp
{
    Temp *next;
    int no;
    int offset;
    //for codegen use
    int reg;//means a reg(index from 1),0 means not in a reg
    int inmemory;//0 means not in memory ,others mean in memory
};

typedef struct ConstVal ConstVal;
struct ConstVal
{
    ConstVal *next;
    int val;
    // for codegen use
    int reg;//means a reg(index from 1),0 means not in a reg
};


// AST node
typedef struct Node Node;
struct Node
{
    NodeKind kind;

    Node *lhs;      //  left head side
    Node *rhs;      //  right head side
    Node *next;    //next state
    ConstVal * constval;       //used if kind == ND_NUM
    Temp *temp;     //used if kind is a operator
    Var *var;

    //if  and while state
    Node *cond;
    Node * then;
    Node *els;

    //for state (add)
    Node * init;
    Node *inc;

    // {...}
    Node *body;

    //function
    char *funcname;
    Node *args;
};

typedef struct Function Function;
struct Function
{
    Node *node;
    Var *local;
    int local_size;
    Temp *temp;
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
    QK_ASSIGN,  // =
    QK_JEZ,     //如果等于0则跳转
    QK_JMP,     //无条件跳转
    QK_LABEL,   //跳转标志
    QK_CALL,    //call function
    QK_PARAM,
} QuadKind;
typedef struct Quad Quad;
struct Quad
{
    QuadKind op;
    Node *arg1;
    Node *arg2;
    Node *result;
    int label;
};
//mid code set
typedef struct QuadSet QuadSet;
struct QuadSet
{
    Quad *list;
    int capacity;
    int size;
    int local_size;
    int temp_size;
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
Token *consume_ident();

int expect_num();
void expect(char *op);
Token *tokenize();

//function of symtable
Temp *new_temp();
ConstVal *new_const();
void print_tempaddr(FILE *out, Temp *temp);

//funtion of parse
Function *program();



//funtion of quadgen
void gen_quadset(Function*func);
void print_quadset();

//funtion of codegen
void gen_code();

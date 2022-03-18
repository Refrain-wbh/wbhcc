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

typedef struct Type Type;

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
    NK_VAR,   // id
    NK_ASSIGN,  // =
    NK_IF,      //if
    NK_WHILE,   //while
    NK_FOR,     //for
    NK_BLOCK,   //block
    NK_FUNCTION,//funtion  
    NK_DEREF,   //  &
    NK_ADDR,    //  *
    NK_NULL,
    NK_PTR_ADD, //地址和int相加
    NK_PTR_SUB, //地址和int相减
    NK_PTR_DIFF,//地址和地址相减

} NodeKind;
/*************sym table*****************/
typedef enum
{
    VK_CONST,
    VK_LOCAL,
    VK_TEMP,
} VarKind;
typedef struct Var Var;
struct Var
{
    VarKind kind;
    Type *type;
    Var *next;
    int reg;
    int offset;
    int in_memory;
    int no;
    int val;
    char *name;
};

typedef struct VarList VarList;
struct VarList
{
    Var *var;
    VarList *next;
};



// AST node
typedef struct Node Node;
struct Node
{
    NodeKind kind;
    Node *lhs;      //  left head side
    Node *rhs;      //  right head side
    Node *next;    //next state

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

    //functioncall
    char *funcname;//used for function call
    Node *args;
    Type *ret_type;

    Token *tok;
};

typedef struct Function Function;
struct Function
{
    Node *node;
    Var *local;
    Type *type;
    int local_size;
    Function *next;
    char *name;
    VarList *params;
};

extern Function *funcList;
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
    QK_DEREF,
    QK_ADDR,
    QK_INT2LONG,
} QuadKind;
typedef struct Quad Quad;
struct Quad
{
    QuadKind op;
    Var *lhs;
    Var *rhs;
    Var *result;

    int label;
    int param_no;
    char *func_name;

    
};
//mid code set
typedef struct QuadSet QuadSet;
struct QuadSet
{
    Quad *list;  //四元式集合
    int capacity;//list的容量
    int size;       //当前四元式数量
    int local_size; //局部变量占据的堆栈大小
    int temp_size;  //临时变量占据的堆栈大小
    char *name;     //function‘s name  一个quadset对应一个function
    VarList *params; // function's params
    QuadSet *next;
};

// type 

typedef enum{TY_INT,TY_LONG,TY_PTR} TypeKind;
struct Type
{
    TypeKind kind;
    int width;
    Type *base;
};

extern Type *int_type;
extern Type *long_type;
extern Type *ptr_type;
//gobal var

extern Token *curtoken;
extern char *user_input;

extern FILE *errout;
extern FILE *quadout;
extern FILE *codeout;



extern QuadSet *quadset;

extern Var *constList;

// funtion of tokenize
bool at_eof();
Token* consume(char* op);
Token *consume_ident();
Token *peek(char *op);
Token *expect_ident();
void error_tok(Token *tok, char *fmt, ...);

int expect_num();
void expect(char *op);
Token *tokenize();

char *strndup(const char *str, int len);

//function of symtable
Var *new_const();


//funtion of parse
Function *program();
Var *new_iconst(int val);


//funtion of quadgen
QuadSet* gen_quadsets(Function*funclist);
void print_quadset();

//funtion of codegen
void gen_code();


//type function
bool is_integer(Type *ty);
bool is_pointer(Type *ty);
bool is_same(Type*ty1,Type*ty2);
Type *point_to(Type *base);
void add_type(Node *node);

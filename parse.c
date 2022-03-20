#include"wcc.h"

static Var *localList;
static int local_offset;
Function *funcList;
Var *constList;

/*####################parse##################*/
static Var *find_var(Token*tok);
static Var *new_var(Type* type,Token*tok);
Var *new_iconst(int val);
static Function *find_func(Token *tok);

static Node *new_node(NodeKind kind,Token*tok);
static Node *new_num(int val);
static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs,Token*tok);
static Node *new_unary(NodeKind kind,Node*expr,Token*tok);
static Node *new_ident(Var *var,Token*tok);

Function *program();
static Function *function();
static VarList *func_params();
static Type *basetype();
static Type *read_type_suffix(Type *base);
static Node *declaration();
static Node *args();
static Node *stmt();
static Node *expr();
static Node *assign();
static Node *equality();
static Node *relational();
static Node *mul();
static Node * unary();
static Node *add();
static Node *primary();
static Node *suffix();

// program := function*
Function * program()
{
    while(!at_eof())
    {
        Function *func = function();
    }
    return funcList;
}
// function := basetype ident "(" func_params? ")" "{" stmt* "}"
Function *function()
{
    localList = NULL;
    local_offset = 0;

    Node head = {};
    Node *cur = &head;

    Type *type = basetype();
    Token * ident = expect_ident();
    VarList *params = NULL;
    expect("(");
    if(!consume(")"))
        params = func_params();
    
    Function *func = calloc(1, sizeof(Function));
    func->name = strndup(ident->str,ident->strlen);
    func->params = params;
    func->type = type;
    
    func->next = funcList;
    funcList = func;

    expect("{");
    
    while(!consume("}"))
    {
        cur->next = stmt();
        cur = cur->next;
    }

    func->node = head.next;
    func->local = localList;
    func->local_size = local_offset;

    return func;
}

static Type *read_type_suffix(Type *base) 
{
    if(!consume("["))
        return base;
    int number = expect_num();
    expect("]");
    base = read_type_suffix(base);
    return array_of(base, number);
}

//basetype := "int" "*"*
static Type * basetype()
{
    expect("int");
    Type * ty = int_type;
    while(consume("*"))
        ty = point_to(ty);
    return ty;
}
// func_params := basetype ident("[" num "]")? (,basetype ident ("[" num "]")? )*
static VarList * func_params()
{
    VarList *params = calloc (1, sizeof(VarList));
    VarList *cur = params;

    Type *ty = basetype();
    Token *tok = expect_ident();
    ty = read_type_suffix(ty);
    cur->var = new_var(ty,tok);
    
    while(!consume(")"))
    {
        expect(",");
        cur->next = calloc(1, sizeof(VarList));
        cur = cur->next;
        ty = basetype();

        Token *tok = expect_ident();
        ty = read_type_suffix(ty);

        cur->var = new_var(ty,tok);
    }

    return params;
}

//  stmt := expr ";"
//       |  "if" "(" expr ")" stmt ("else" "stmt")?
//       |  "while" "(" expr ")" stmt
//       |  "for" "(" expr? ";" expr? ";" expr? ";" ")"stmt
//       | "return" expr ";"
//       | "{" stmt* "}"
//       | declaration
static Node*stmt()
{
    Token *tok;
    if(tok=consume("return"))
    {
        Node *node = new_unary(NK_RETURN, expr(),tok);
        expect(";");
        return node;
    }
    if(tok=consume("if"))
    {
        Node *node = new_node(NK_IF,tok);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        if(consume("else"))
            node->els = stmt();
        return node;
    }
    if(tok=consume("while"))
    {
        Node *node = new_node(NK_WHILE,tok);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        return node;
    }
    if(tok=consume("for"))
    {
        Node *node = new_node(NK_FOR,tok);
        expect("(");
        if (!consume(";"))
        {
            node->init = expr();
            expect(";");
        }
        if (!consume(";"))
        {
            node->cond = expr();
            expect(";");
        }
        if(!consume(")"))
        {
            node->inc = expr();
            expect(")");
        }
        node->then = stmt();
        return node;
    }
    if(tok=consume("{"))
    {
        Node head = {};
        Node *cur = &head;
        while(!consume("}"))
        {
            cur->next = stmt();
            cur = cur->next;
        }
        Node *node = new_node(NK_BLOCK,tok);
        node->body = head.next;
        return node;
    }
    if(peek("int"))
        return declaration();
    Node *node = expr();
    expect(";");
    return node;
}
// declaration := basetype ident("[" num  "]") ("=" expr)";"
static Node * declaration()
{
    Token *tok;
    Type *type = basetype();
    tok = expect_ident();
    type = read_type_suffix(type);

    Var *lvar = new_var(type, tok);

    if (consume(";"))
    {
        return new_node(NK_NULL, tok);
    }
    Node *lnode = new_ident(lvar, tok);
    expect("=");
    Node *rnode = expr();
    expect(";");
    return new_binary(NK_ASSIGN, lnode, rnode, tok);
}

// expr := assign
static Node * expr()
{
    return assign();
}
// assign := equality ("=" assign)?
static Node * assign()
{
    Node *node = equality();
    Token *tok;
    if(tok=consume("="))
        return new_binary(NK_ASSIGN, node, assign(),tok);
    
    return node;
}

//equality:=relational("==" relational | "!=" relational) *
static Node * equality()
{
    Node *node = relational();
    while(1)
    {
        Token *tok;
        if(tok=consume("=="))
            node = new_binary(NK_EQ, node, relational(),tok);
        else if(tok=consume("!="))
            node = new_binary(NK_NE, node, relational(),tok);
        else 
            return node;
    }
}
//relational:=add(">=" add | "<=" add | ">" add | "<" add) *
static Node * relational()
{
    Node *node = add();
    while(1)
    {
        Token *tok;
        if(tok=consume("<="))
            node = new_binary(NK_LE, node, add(),tok);
        else if(tok=consume(">="))
            node = new_binary(NK_LE, add(), node,tok);
        else if(tok=consume("<"))
            node = new_binary(NK_LT, node, add(),tok);
        else if(tok=consume(">"))
            node = new_binary(NK_LT, add(), node,tok);
        else
            return node;
    }
}
//add:=mul("+" mul | "-" mul)*
static Node *add()    
{
    Node *node = mul();
    while(1)
    {
        Token *tok;
        if(tok=consume("+"))
            node = new_binary(NK_ADD, node, mul(), tok);
        else if(tok=consume("-"))
            node = new_binary(NK_SUB, node, mul(), tok);
        else
            return node;
    }
}

// mul := unary ("*" unary | "/" unary)*
static Node * mul()
{
    Node *node = unary();
    while(1)
    {
        Token *tok;
        if(tok=consume("*"))
            node = new_binary(NK_MUL, node, unary(),tok);
        else if(tok=consume("/"))
            node = new_binary(NK_DIV, node, unary(),tok);
        else
            return node;
    }
}

// unary := ("+" | "-" | "*" | "&")? unary  
//       | suffix
static Node*unary()
{
    Token *tok;
    if(consume("+"))
        return unary();
    if(tok=consume("-"))
        return new_binary(NK_SUB, new_num(0), unary(),tok);
    if(tok=consume("*"))
        return new_unary(NK_DEREF, unary(),tok);
    if(tok=consume("&"))
        return new_unary(NK_ADDR, unary(),tok);
    return suffix();
}


// suffix := primary ("[" expr "]")*
static Node * suffix()
{
    Node *node = primary();
    Token *tok;
    while((tok = consume("[")))
    {
        Node *e = expr();
        node = new_binary(NK_ADD, node, e, tok);
        node = new_unary(NK_DEREF, node, tok);
        expect("]");
    }
    return node;
}

//primary:=num | "(" expr ")" | identity (args)?
static Node * primary()
{
    if(consume("("))
    {
        Node *node = expr();
        expect(")");
        return node;
    }
    Token*tok;
    if((tok=consume_ident()))
    {
        if(consume("("))//means it is a function
        {
            Node *node = new_node(NK_FUNCTION,tok);
            Function *func = find_func(tok);
            if(func==NULL)
                error_tok(tok, "Undeclared function");

            node->ret_type = func->type;
            node->funcname = strndup(tok->str, tok->strlen);
            node->args = args();
            return node;
        }
        Var *var = find_var(tok);
        if(var==NULL)
            error_tok(tok, "undefined variable");
        return new_ident(var,tok);
    }
    return new_num(expect_num());
}


//  args:= "(" ( assign  ("," assign)* )? ")"
//args 的左括号已经被读入
static Node * args()
{
    if(consume(")"))
        return NULL;
    Node *node = assign();
    Node *cur = node;
    while(consume(","))
    {
        cur->next = assign();
        cur = cur->next;
    }
    expect(")");
    return node;
}




static Node *new_node(NodeKind kind,Token*tok)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;
}

//calloc one AST Node for integer num
static Node * new_num(int val)
{
    Node *node = new_node(NK_NUM,NULL);
    node->var = new_iconst(val);
    return node;
}
//calloc one AST Node for two-operand operator 
static Node * new_binary(NodeKind kind,Node*lhs,Node*rhs,Token*tok)
{
    Node *node = new_node(kind,tok);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}
static Node *new_unary(NodeKind kind,Node*expr,Token*tok)
{
    Node *node = new_node(kind,tok);
    node->lhs = expr;
    return node;
}

static Node *new_ident(Var * var,Token*tok)
{
    Node *node = new_node(NK_VAR,tok);
    node->var = var;
    return node;
}

static Var *find_var(Token*tok)
{
    for (Var *var = localList; var;var=var->next)
        if(tok->strlen==strlen(var->name) && 
            strncmp(var->name,tok->str,tok->strlen)==0)
            return var;
    return NULL;
}
static Var *new_var(Type* type,Token*tok)
{
    
    Var *var = calloc(1, sizeof(Var));
    var->kind = VK_LOCAL;
    var->type = type;
    var->name = strndup(tok->str,tok->strlen);
    var->next = localList;
    local_offset += type->size;
    var->offset = local_offset;
    localList = var;
    return var;
}

static Var *find_iconst(int val)
{
    for (Var *var = constList; var; var=var->next)
        if(var->val == val)
            return var;
    return NULL;
}
static Function *find_func(Token *tok)
{
    for (Function *func = funcList; func;func=func->next)
        if(tok->strlen==strlen(func->name) && 
            strncmp(func->name,tok->str,tok->strlen)==0)
            return func;
    return NULL;
}
Var *new_iconst(int val)
{
    Var *newnode = find_iconst(val);
    if(newnode)
        return newnode;
    newnode = calloc(1, sizeof(Var));
    newnode->kind = VK_CONST;
    newnode->next = constList;
    newnode->val = val;
    newnode->type = int_type;
    constList = newnode;
    return newnode;
}


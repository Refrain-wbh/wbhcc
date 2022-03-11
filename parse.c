#include"wcc.h"

static Var *local;
static int local_offset;
static Temp *temp;
static ConstVal *constval;

/*####################parse##################*/
static Var *find_var(Token*tok);
static Var *new_ivar(Token*tok);

static Node *new_node(NodeKind kind,Token*tok);
static Node *new_num(long val);
static Node *new_binary_op(NodeKind kind, Node *lhs, Node *rhs,Token*tok);
static Node *new_unary(NodeKind kind,Node*expr,Token*tok);
static Node *new_unary_op(NodeKind kind, Node *expr, Token *tok);
static Node *new_ident(Var *var,Token*tok);

Function *program();
static Function *function();
static VarList *func_params();
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


// program := function*
Function * program()
{
    Function func = {};
    Function *cur = &func;
    while(!at_eof())
    {
        cur->next = function();
        cur = cur->next;
        add_type(cur->node);
    }
    return func.next;
}
// function := ident "(" func_params? ")" "{" stmt* "}"
Function *function()
{
    local = NULL;
    temp = NULL;
    local_offset = 0;

    Node head = {};
    Node *cur = &head;
    
    Token * ident = expect_ident();
    VarList *params = NULL;
    expect("(");
    if(!consume(")"))
        params = func_params();
    
    expect("{");
    
    while(!consume("}"))
    {
        cur->next = stmt();
        cur = cur->next;
    }

    
    Function *func = calloc(1, sizeof(Function));
    func->name = strndup(ident->str,ident->strlen);
    func->node = head.next;
    func->local = local;
    func->temp = temp;
    func->local_size = local_offset;
    func->params = params;
    return func;
}
// func_params := ident(,ident)*
static VarList * func_params()
{
    VarList *params = calloc (1, sizeof(VarList));
    VarList *cur = params;
    
    cur->var = new_ivar(expect_ident());
    
    while(!consume(")"))
    {
        expect(",");
        cur->next = calloc(1, sizeof(VarList));
        cur = cur->next;
        cur->var = new_ivar(expect_ident());
    }

    return params;
}

//  stmt := expr ";"
//       |  "if" "(" expr ")" stmt ("else" "stmt")?
//       |  "while" "(" expr ")" stmt
//       |  "for" "(" expr? ";" expr? ";" expr? ";" ")"stmt
//       | "return" expr ";"
//       | "{" stmt* "}"
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
    Node *node = expr();
    expect(";");
    return node;
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
        return new_binary_op(NK_ASSIGN, node, assign(),tok);
    
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
            node = new_binary_op(NK_EQ, node, relational(),tok);
        else if(tok=consume("!="))
            node = new_binary_op(NK_NE, node, relational(),tok);
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
            node = new_binary_op(NK_LE, node, add(),tok);
        else if(tok=consume(">="))
            node = new_binary_op(NK_LE, add(), node,tok);
        else if(tok=consume("<"))
            node = new_binary_op(NK_LT, node, add(),tok);
        else if(tok=consume(">"))
            node = new_binary_op(NK_LT, add(), node,tok);
        else
            return node;
    }
}

static Node*new_add(Node* arg1,Node*arg2,Token*tok)
{
    add_type(arg1);
    add_type(arg2);

    if(is_integer(arg1)&&is_integer(arg2))
        return new_binary_op(NK_ADD, arg1, arg2, tok);
    if(is_integer(arg1)&&arg2->type->base)
        return new_binary_op(NK_PTR_ADD, arg1, arg2, tok);
    if(is_integer(arg2)&&arg1->type->base)
        return new_binary_op(NK_PTR_ADD, arg1, arg2, tok);
    error_tok(tok, "invalid operands");
}

static Node*new_sub(Node* arg1,Node*arg2,Token*tok)
{
    add_type(arg1);
    add_type(arg2);

    if(is_integer(arg1)&&is_integer(arg2))
        return new_binary_op(NK_SUB, arg1, arg2, tok);
    if(arg1->type->base&&arg2->type->base)
        return new_binary_op(NK_PTR_DIFF, arg1, arg2, tok);
    if(arg1->type->base&&is_integer(arg2->type))
        return new_binary_op(NK_PTR_SUB, arg1, arg2, tok);
    error_tok(tok, "invalid operands");
}
//add:=mul("+" mul | "-" mul)*
static Node *add()    
{
    Node *node = mul();
    while(1)
    {
        Token *tok;
        if(tok=consume("+"))
            node=new_add(node,mul(),tok);
        else if(tok=consume("-"))
            node = new_sub(node,mul(),tok);
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
            node = new_binary_op(NK_MUL, node, unary(),tok);
        else if(tok=consume("/"))
            node = new_binary_op(NK_DIV, node, unary(),tok);
        else
            return node;
    }
}

// unary := ("+" | "-" | "*" | "&")? unary
//       | primary
static Node*unary()
{
    Token *tok;
    if(consume("+"))
        return unary();
    if(tok=consume("-"))
        return new_binary_op(NK_SUB, new_num(0), unary(),tok);
    if(tok=consume("*"))
        return new_unary_op(NK_DEREF, unary(),tok);
    if(tok=consume("&"))
        return new_unary_op(NK_ADDR, unary(),tok);
    return primary();
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
            node->temp = new_temp();
            node->funcname = strndup(tok->str, tok->strlen);
            node->args = args();
            return node;
        }
        Var *var = find_var(tok);
        if(var==NULL)
            var = new_ivar(tok);
        return new_ident(var,tok);
    }
    return new_num(expect_num());
}



static Node *new_node(NodeKind kind,Token*tok)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;
}

//calloc one AST Node for integer num
static Node * new_num(long val)
{
    Node *node = new_node(NK_NUM,NULL);
    node->constval = new_const();
    node->constval->val = val;
    return node;
}
//calloc one AST Node for two-operand operator 
static Node * new_binary_op(NodeKind kind,Node*lhs,Node*rhs,Token*tok)
{
    Node *node = new_node(kind,tok);
    node->lhs = lhs;
    node->rhs = rhs;
    node->temp = new_temp();
    return node;
}
static Node *new_unary(NodeKind kind,Node*expr,Token*tok)
{
    Node *node = new_node(kind,tok);
    node->lhs = expr;
    return node;
}

static Node *new_unary_op(NodeKind kind,Node*expr,Token*tok)
{
    Node *node = new_node(kind,tok);
    node->lhs = expr;
    node->temp = new_temp();
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
    for (Var *var = local; var;var=var->next)
        if(tok->strlen==strlen(var->name) && 
            strncmp(var->name,tok->str,tok->strlen)==0)
            return var;
    return NULL;
}
static Var *new_ivar(Token*tok)
{
    
    Var *var = calloc(1, sizeof(Var));
    var->name = strndup(tok->str,tok->strlen);
    var->next = local;
    local_offset += 4;
    var->offset = local_offset;
    local = var;
    return var;
}

Temp *new_temp()
{
    static int tempnum = 0;
    Temp *newnode = calloc(1, sizeof(Temp));
    newnode->next = temp;
    temp = newnode;
    temp->no = tempnum++;
    return temp;
}

ConstVal *new_const()
{
    ConstVal *newnode = calloc(1, sizeof(ConstVal));
    newnode->next = constval;
    constval = newnode;
    return constval;
}


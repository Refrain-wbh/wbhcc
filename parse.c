#include"wcc.h"

static Var *local;
static int local_offset;
static Temp *temp;
static ConstVal *constval;

/*####################parse##################*/
static Var *find_var(const char * id);
static Var *new_ivar(char *name);

static Node *new_node(NodeKind kind);
static Node *new_num(long val);
static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs);
static Node *new_unary(NodeKind kind,Node*expr);
static Node *new_ident(Var *var);

Function *program();
static Function *function();
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
    }
    return func.next;
}
// function := ident "(" ")" "{" stmt* "}"
Function *function()
{
    local = NULL;
    temp = NULL;
    local_offset = 0;

    Node head = {};
    Node *cur = &head;
    
    char * funcname = expect_ident();
    expect("(");
    expect(")");
    expect("{");
    
    while(!consume("}"))
    {
        cur->next = stmt();
        cur = cur->next;
    }
    
    Function *func = calloc(1, sizeof(Function));
    func->name = funcname;
    func->node = head.next;
    func->local = local;
    func->temp = temp;
    func->local_size = local_offset;
    return func;
}
//  stmt := expr ";"
//       |  "if" "(" expr ")" stmt ("else" "stmt")?
//       |  "while" "(" expr ")" stmt
//       |  "for" "(" expr? ";" expr? ";" expr? ";" ")"stmt
//       | "return" expr ";"
//       | "{" stmt* "}"
static Node*stmt()
{
    if(consume("return"))
    {
        Node *node = new_unary(NK_RETURN, expr());
        expect(";");
        return node;
    }
    if(consume("if"))
    {
        Node *node = new_node(NK_IF);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        if(consume("else"))
            node->els = stmt();
        return node;
    }
    if(consume("while"))
    {
        Node *node = new_node(NK_WHILE);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        return node;
    }
    if(consume("for"))
    {
        Node *node = new_node(NK_FOR);
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
    if(consume("{"))
    {
        Node head = {};
        Node *cur = &head;
        while(!consume("}"))
        {
            cur->next = stmt();
            cur = cur->next;
        }
        Node *node = new_node(NK_BLOCK);
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
    if(consume("="))
        return new_binary(NK_ASSIGN, node, assign());
    return node;
}

//equality:=relational("==" relational | "!=" relational) *
static Node * equality()
{
    Node *node = relational();
    while(1)
    {
        if(consume("=="))
            node = new_binary(NK_EQ, node, relational());
        else if(consume("!="))
            node = new_binary(NK_NE, node, relational());
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
        if(consume("<="))
            node = new_binary(NK_LE, node, add());
        else if(consume(">="))
            node = new_binary(NK_LE, add(), node);
        else if(consume("<"))
            node = new_binary(NK_LT, node, add());
        else if(consume(">"))
            node = new_binary(NK_LT, add(), node);
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
        if(consume("+"))
            node=new_binary(NK_ADD,node,mul());
        else if(consume("-"))
            node = new_binary(NK_SUB, node, mul());
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
        if(consume("*"))
            node = new_binary(NK_MUL, node, unary());
        else if(consume("/"))
            node = new_binary(NK_DIV, node, unary());
        else
            return node;
    }
}

// unary := ("+" | "-")? unary
//       | primary
static Node*unary()
{
    if(consume("+"))
        return unary();
    if(consume("-"))
        return new_binary(NK_SUB, new_num(0), unary());
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
    char *id;
    if((id=consume_ident())!=NULL)
    {
        if(consume("("))//means it is a function
        {
            Node *node = new_node(NK_FUNCTION);
            node->temp = new_temp();
            node->funcname = id;
            node->args = args();
            return node;
        }
        Var *var = find_var(id);
        if(var==NULL)
            var = new_ivar(id);
        return new_ident(var);
    }
    return new_num(expect_num());
}



static Node *new_node(NodeKind kind)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

//calloc one AST Node for integer num
static Node * new_num(long val)
{
    Node *node = new_node(NK_NUM);
    node->constval = new_const();
    node->constval->val = val;
    return node;
}
//calloc one AST Node for two-operand operator 
static Node * new_binary(NodeKind kind,Node*lhs,Node*rhs)
{
    Node *node = new_node(kind);
    node->lhs = lhs;
    node->rhs = rhs;
    node->temp = new_temp();
    return node;
}
static Node *new_unary(NodeKind kind,Node*expr)
{
    Node *node = new_node(kind);
    node->lhs = expr;
    return node;
}
static Node *new_ident(Var * var)
{
    Node *node = new_node(NK_IDENT);
    node->var = var;
    return node;
}


static Var *find_var(const char * id)
{
    for (Var *var = local; var;var=var->next)
        if(strlen(id)==strlen(var->name) && 
            strcmp(var->name,id)==0)
            return var;
    return NULL;
}
static Var *new_ivar(char *name)
{
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
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


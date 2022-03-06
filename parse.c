#include"wcc.h"

/*####################parse##################*/
static Node *new_num();
static Node *new_binary();


Node *expr();
static Node *primary();
static Node *mul();

//expr:=mul("+" mul | "-" mul)*
Node *expr()
{
    Node *node = mul();
    while(1)
    {
        if(consume('+'))
            node=new_binary(NK_ADD,node,mul());
        else if(consume('-'))
            node = new_binary(NK_SUB, node, mul());
        else
            return node;
    }
}
//mul:=primary("*" primary | "/" primary)*
Node * mul()
{
    Node *node = primary();
    while(1)
    {
        if(consume('*'))
            node = new_binary(NK_MUL, node, primary());
        else if(consume('/'))
            node = new_binary(NK_DIV, node, primary());
        else
            return node;
    }
}
//primary:=num | "(" expr ")"
Node * primary()
{
    if(consume('('))
    {
        Node *node = expr();
        expect(')');
        return node;
    }
    return new_num(expect_num());
}

//calloc one AST Node for integer num
Node * new_num(long val)
{
    Node *node = calloc(1, sizeof(Node));
    node->constval = new_const();
    node->constval->val = val;
    node->kind = NK_NUM;
    return node;
}

//calloc one AST Node for two-operand operator 
Node * new_binary(NodeKind kind,Node*lhs,Node*rhs)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    node->temp = new_temp();
    return node;
}

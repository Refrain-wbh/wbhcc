#include"wcc.h"

#define print(fmt,...) fprintf(quadout,fmt,## __VA_ARGS__)


static int new_label()
{
    static int label = 0;
    return label++;
}

static void enlarge_capacity()
{
    int newcap = quadset->capacity!=0 ? quadset->capacity * 2 : 500;
    Quad *newlist = calloc(newcap, sizeof(Quad));
    if(quadset->capacity!=0)
    {
        memcpy(newlist, quadset->list, quadset->size * sizeof(Quad));
        free(quadset->list);
    }
    quadset->list = newlist;
    quadset->capacity = newcap;
}
static Quad* new_quad()
{
    int idx = quadset->size;
    if(idx>=quadset->capacity)
        enlarge_capacity();
    
    quadset->size++;
    return quadset->list + idx;
}


//使用一种简单的方式计算temp的offset
//目前来说，四元式的arg1 arg2是使用处，result为定义处，return中的arg1是expr
//assign则左侧是equality，因此可以直接遍历四元式的temp
//返回temp占用的总大小
static int calc_temp_offset()
{
    int offset = 0;
    int maxoffset = 0;
    for (int i = 0; i < quadset->size;++i)
    {
        Quad *quad = &quadset->list[i];
        Temp *arg1 = quad->arg1 ? quad->arg1->temp : NULL;
        Temp *arg2 = quad->arg2 ? quad->arg2->temp : NULL;
        Temp *result = quad->result ? quad->result->temp : NULL;
        
        if(arg1)
            offset -= 4;
        if(arg2)
            offset -= 4;
        if(result)
            result->offset = (offset += 4);
        maxoffset = maxoffset > offset ? maxoffset : offset;
    }
    return maxoffset;
}


static Quad* gen_jump(QuadKind kind,Node*arg1,Node*arg2,int label)
{
    Quad *quad = new_quad();

    quad->arg1 = arg1;
    quad->arg2 = arg2;
    quad->op = kind;
    quad->label = label;
    return quad;
}
static Quad* gen_label(int label)
{
    Quad *quad = new_quad();

    quad->op = QK_LABEL;
    quad->label = label;
    return quad;
}

//func 中有参数，这些参数要计算出来,然后作为参数传递进去
static Quad * gen_call(Node * func)
{
    // 传递参数
    for (Node *cur = func->args; cur;cur = cur->next)
    {
        Quad * quad = new_quad();
        quad->op = QK_PARAM;
        quad->result = cur;
    }
    Quad *quad = new_quad();
    quad->op = QK_CALL;
    quad->result = func;
    return quad;
}

//operation,so must have a result (temp var) in result-node
static Quad * gen_operation(NodeKind kind,Node*arg1,Node*arg2,Node*result)
{
    Quad *quad = new_quad();

    quad->arg1 = arg1;
    quad->arg2 = arg2;
    quad->result = result;
    

    switch (kind)
    {
    case NK_ADD:
        quad->op = QK_ADD;
        break;
    case NK_SUB:
        quad->op = QK_SUB;
        break;
    case NK_DIV:
        quad->op = QK_DIV;
        break;
    case NK_MUL:
        quad->op = QK_MUL;
        break;
    case NK_LE:
        quad->op = QK_LE;
        break;
    case NK_EQ:
        quad->op = QK_EQ;
        break;
    case NK_NE:
        quad->op = QK_NE;
        break;
    case NK_LT:
        quad->op = QK_LT;
        break;
    case NK_RETURN:
        quad->op = QK_RETURN;
        break;
    case NK_ASSIGN:
        quad->op = QK_ASSIGN;
        break;
    default:
        break;
    }
    return quad;
}

void gen_quadcode(Node *ASTroot)
{
    switch (ASTroot->kind)
    {
    case NK_ADD:
    case NK_SUB:
    case NK_DIV:
    case NK_MUL:
    case NK_LE:
    case NK_EQ:
    case NK_NE:
    case NK_LT:
        gen_quadcode(ASTroot->lhs);
        gen_quadcode(ASTroot->rhs);
        gen_operation(ASTroot->kind, ASTroot->lhs, ASTroot->rhs, ASTroot);
        break;
    case NK_RETURN:
        gen_quadcode(ASTroot->lhs);
        gen_operation(ASTroot->kind, ASTroot->lhs, NULL, NULL);
        break;
    case NK_ASSIGN:
        gen_quadcode(ASTroot->lhs);
        gen_quadcode(ASTroot->rhs);
        gen_operation(ASTroot->kind, ASTroot->rhs, NULL, ASTroot->lhs);
        break;
    case NK_IF:
        if (ASTroot->els)
        {
            int else_label = new_label(), end_label = new_label();
            gen_quadcode(ASTroot->cond);
            gen_jump(QK_JEZ, ASTroot->cond, NULL, else_label);
            gen_quadcode(ASTroot->then);
            gen_jump(QK_JMP, NULL, NULL, end_label);
            gen_label(else_label);
            gen_quadcode(ASTroot->els);
            gen_label(end_label);
        }
        else
        {
            int end_label = new_label();
            gen_quadcode(ASTroot->cond);
            gen_jump(QK_JEZ, ASTroot->cond, NULL, end_label);
            gen_quadcode(ASTroot->then);
            gen_label(end_label);
        }
        break;
    case NK_WHILE:
    {
        int begin_label = new_label(), end_label = new_label();
        gen_label(begin_label);
        gen_quadcode(ASTroot->cond);
        gen_jump(QK_JEZ, ASTroot->cond, NULL, end_label);
        gen_quadcode(ASTroot->then);
        gen_jump(QK_JMP, NULL, NULL, begin_label);
        gen_label(end_label);

        break;
    }
    case NK_FOR:
    {
        if (ASTroot->init)
            gen_quadcode(ASTroot->init);
        int begin_label = new_label(), end_label = new_label();
        gen_label(begin_label);

        if (ASTroot->cond)
        {
            gen_quadcode(ASTroot->cond);
            gen_jump(QK_JEZ, ASTroot->cond, NULL, end_label);
        }
        gen_quadcode(ASTroot->then);
        if (ASTroot->inc)
            gen_quadcode(ASTroot->inc);
        gen_jump(QK_JMP, NULL, NULL, begin_label);
        gen_label(end_label);

        break;
    }
    case NK_BLOCK:
    {
        for (Node *cur = ASTroot->body; cur; cur = cur->next)
            gen_quadcode(cur);
        break;
    }
    case NK_FUNCTION:
    {
        //function 节点
        for (Node *cur = ASTroot->args; cur;cur=cur->next)
            gen_quadcode(cur);
        
        gen_call(ASTroot);
        break;
    }
    default:
        break;
    }
}
void gen_quadset(Function*function)
{
    for (Node *cur = function->node; cur;cur=cur->next)
        gen_quadcode(cur);

    int tempsize=calc_temp_offset();
    quadset->temp_size = tempsize;
    quadset->local_size = function->local_size;
}

// print const or var or temp
static void print_addr(Node*node)
{
    if(node==NULL)
        return;
    if (node->kind == NK_NUM)
    {
        print("%d", node->constval->val);
    }
    else if(node->kind== NK_IDENT)
    {
        print("%s", node->var->name);
    }
    else
    {
        print("T%d", node->temp->no);
    }
}
// print z=x op y  or z=op x
static void print_operation(Quad*quad,int no)
{
    print("\t%d\t:(",no);
    switch(quad->op)
    {
    case QK_ADD:
        print("+");
        break;
    case QK_SUB:
        print("-");
        break;
    default:
        break;
    }
    
    print(",");
    print_addr(quad->arg1);
    print(",");
    print_addr(quad->arg2);
    print(",");
    print_addr(quad->result);
    print(")\n");
}
void print_quadset()
{
    for (int i = 0; i < quadset->size; ++i)
    {
        
        Quad *cur = &quadset->list[i];
        switch (cur->op)
        {
        case QK_ADD:
        case QK_SUB:
            print_operation(cur, i);
            break;
        default:
            break;
        }
    }
}
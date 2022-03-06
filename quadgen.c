#include"wcc.h"

#define print(fmt,...) fprintf(quadout,fmt,## __VA_ARGS__)

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

//operation,so must have a result (temp var) in result-node
static int gen_operation(NodeKind kind,Node*arg1,Node*arg2,Node*result)
{
    int idx = quadset->size;
    if(idx>=quadset->capacity)
    {
        enlarge_capacity();
    }
    quadset->size++;
    quadset->list[idx].arg1 = arg1;
    quadset->list[idx].arg2 = arg2;
    quadset->list[idx].result = result;

    Quad *cur = quadset->list + idx;
    switch (kind)
    {
    case NK_ADD:
        cur->op = QK_ADD;
        break;
    case NK_SUB:
        cur->op = QK_SUB;
        break;
    default:
        break;
    }
    return idx;
}
void gen_quadset(Node*ASTroot)
{
    switch (ASTroot->kind)
    {
    case NK_ADD:
    case NK_SUB:
        gen_quadset(ASTroot->lhs);
        gen_quadset(ASTroot->rhs);
        gen_operation(ASTroot->kind, ASTroot->lhs, ASTroot->rhs, ASTroot);
        break;
    default:
        break;
    }
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
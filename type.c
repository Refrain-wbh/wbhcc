#include "wcc.h"
Type *int_type = &(Type){TY_INT, 4};
Type *ptr_type = &(Type){TY_PTR, 8};
bool is_integer(Type *ty)
{
    return ty->kind == TY_INT;
}
Type *point_to(Type *base)
{
    Type *type = calloc(1, sizeof(Type));
    type->base = base;
    type->kind = TY_PTR;
    type->width = 8;
}
void add_type(Node *node)
{
    if (!node || node->type)
        return;

    add_type(node->lhs);
    add_type(node->rhs);
    add_type(node->cond);
    add_type(node->then);
    add_type(node->els);
    add_type(node->init);
    add_type(node->inc);

    for (Node *n = node->next; n;n=n->next)
        add_type(n);
    for (Node *n = node->body; n; n = n->next)
        add_type(n);
    for (Node *n = node->args; n; n = n->next)
        add_type(n);

    switch (node->kind)
    {
    case NK_ADD:
    case NK_SUB:
    case NK_MUL:
    case NK_DIV:
    case NK_EQ:
    case NK_NE:
    case NK_LT:
    case NK_LE:
    case NK_VAR:
    case NK_FUNCTION:
    case NK_NUM:
        node->ty = int_type;
        break;
    case NK_ASSIGN:
        node->ty = node->lhs->ty;
        break;
    case NK_ADDR:
        node->ty = pointer_to(node->lhs->ty);
        break;
    case NK_DEREF:
        if (node->lhs->type->kind == TY_PTR)
            node->ty = node->lhs->ty->base;
        else
            node->ty = int_type;
    default:
        break;
    }
    return;
}
}
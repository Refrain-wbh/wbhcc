#include"wcc.h"

#define print(fmt,...) fprintf(quadout,fmt,## __VA_ARGS__)
static Var*new_temp(Type*type);
static Function *cur_func;
static Var *tempList;

Var *gen_expr(Node *node);
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
        int lhs_w = quad->lhs &&quad->lhs->kind == VK_TEMP ? quad->lhs->type->width : 0;
        int rhs_w = quad->rhs &&quad->rhs->kind == VK_TEMP? quad->rhs->type->width : 0;
        int tar_w = quad->result &&quad->result->kind == VK_TEMP? quad->result->type->width : 0;

        offset -= lhs_w + rhs_w;
        
        if(quad->result &&quad->result->kind == VK_TEMP)
            quad->result->offset = (offset += tar_w) + cur_func->local_size;
        maxoffset = maxoffset > offset ? maxoffset : offset;
    }
    return maxoffset;
}

static Var*new_temp(Type*type)
{
    static int no = 0;
    Var *node = calloc(1, sizeof(Var));
    node->type = type;
    node->kind = VK_TEMP;
    node->no = no++;
    node->next = tempList;
    tempList = node;
    return node;
}

static Var * gen_binary(QuadKind kind,Var *lhs,Var*rhs,Var*result)
{
    Quad *quad = new_quad();
    quad->op = kind;
    quad->lhs = lhs;
    quad->rhs = rhs;
    quad->result = result;
    return result;
}
static Var * gen_unary(QuadKind kind,Var *srs,Var *tar)
{
    Quad *quad = new_quad();
    quad->op = kind;
    quad->lhs = srs;
    quad->result = tar;
    return tar;
}
void gen_param(Var* var,int param_no)
{
    Quad *quad = new_quad();
    quad->op = QK_PARAM;
    quad->lhs = var;
    quad->param_no = param_no;
}
Var * gen_add(Var*lhs,Var*rhs,Token*tok)
{
    if(is_integer(lhs->type) && is_integer(rhs->type))
        return gen_binary(QK_ADD, lhs, rhs, new_temp(int_type));
    if(is_integer(lhs->type) && is_pointer(rhs->type))
    {
        Var *tol = gen_unary(QK_INT2LONG, lhs, new_temp(long_type));
        Var *lens = gen_binary(QK_MUL, tol,
                               new_iconst(rhs->type->base->width),
                               new_temp(long_type));
        return gen_binary(QK_ADD, rhs, lens, new_temp(rhs->type));
    }
    if(is_integer(rhs->type) && is_pointer(lhs->type))
    {
        Var *tol = gen_unary(QK_INT2LONG, rhs, new_temp(long_type));
        Var *lens = gen_binary(QK_MUL, tol,
                               new_iconst(lhs->type->base->width),
                               new_temp(long_type));
        return gen_binary(QK_ADD, lhs, lens, new_temp(lhs->type));
    }
    error_tok(tok, "invalid operand{int + int} or {pointer + int}or {int + pointer}");
}
Var * gen_sub(Var*lhs,Var*rhs,Token*tok)
{
    if(is_integer(lhs->type) && is_integer(rhs->type))
        return gen_binary(QK_SUB, lhs, rhs, new_temp(int_type));
    if(is_integer(rhs->type) && is_pointer(lhs->type))
    {
        Var *tol = gen_unary(QK_INT2LONG, rhs, new_temp(long_type));
        Var *lens = gen_binary(QK_MUL, tol,
                               new_iconst(lhs->type->base->width),
                               new_temp(long_type));
        return gen_binary(QK_SUB, lhs, lens, new_temp(lhs->type));
    }
    if(is_pointer(lhs->type) && is_pointer(rhs->type))
    {
        if(!is_same(lhs->type,rhs->type))
        {
            error_tok(tok, "base of pointer needed be same");
        }
        Var * result = gen_binary(QK_SUB, lhs, rhs, new_temp(lhs->type));
        Var *bediv = new_iconst(lhs->type->base->width);
        bediv = gen_unary(QK_INT2LONG, bediv, new_temp(lhs->type));
        return gen_binary(QK_DIV, result, bediv, new_temp(lhs->type->base));
    }
    error_tok(tok, "invalid operand{int - int} or {pointer - pointer} or {pointer - int}");
}
Var * gen_mul_sub(NodeKind kind ,Var * lhs,Var * rhs,Token*tok)
{
    if(is_integer(lhs->type) && is_integer(rhs->type))
    {
        if(kind == NK_DIV)
            return gen_binary(QK_DIV, lhs, rhs, new_temp(int_type));
        else
            return gen_binary(QK_MUL, lhs, rhs, new_temp(int_type));
    }
    error_tok(tok, "two operator need to be integer");
    
}
Var * gen_cmp(NodeKind kind,Var * lhs,Var * rhs, Token * tok)
{
    if(is_same(lhs->type,rhs->type))
    {
        switch (kind)
        {
        case NK_LE:
            return gen_binary(QK_LE, lhs, rhs, new_temp(int_type));
        case NK_LT:
            return gen_binary(QK_LT, lhs, rhs, new_temp(int_type));
        case NK_EQ:
            return gen_binary(QK_EQ, lhs, rhs, new_temp(int_type));
        case NK_NE:
            return gen_binary(QK_NE, lhs, rhs, new_temp(int_type));
        default:
            return NULL;
        }
    }
    else
        error_tok(tok, "operator type need to be same");
}
Var * gen_assign(Var*lhs,Var * rhs,Token*tok)
{
    if(lhs->type->base==NULL || !is_same(lhs->type->base,rhs->type))
    {
        error_tok(tok, "two side type needed to be same");
    }
    else 
    {
        return gen_unary(QK_ASSIGN, rhs, lhs);
    }
}
Var * gen_get_addr(Node* node)
{
    if(node->kind == NK_DEREF)
        return gen_expr(node->lhs);
    else if(node->kind == NK_VAR)
    {
        return gen_unary(QK_ADDR, node->var, new_temp(point_to(node->var->type)));
    }
    else
    {
        error_tok(node->tok, "not an left value");
    }  
 
}
Var * gen_get_content(Var * var,Token * tok)
{
    if(!is_pointer(var->type))
        error_tok(tok, "needed to be a pointer");
    else 
        return gen_unary(QK_DEREF, var, new_temp(var->type->base));
}
Var * gen_func_call(Node*node)
{
    int no = 1;
    for (Node *cur = node->args; cur;cur=cur->next)
        gen_param(gen_expr(cur), no++);
    Quad *quad = new_quad();
    quad->op = QK_CALL;
    quad->result = new_temp(node->ret_type);
    quad->func_name = node->funcname;

    return quad->result;
}
void gen_jump(QuadKind kind, Var * lhs,Var * rhs,int label)
{
    Quad *quad = new_quad();
    quad->lhs = lhs;
    quad->rhs = rhs;
    quad->label = label;
    quad->op = kind;
}
void gen_label(int label)
{
    Quad *quad = new_quad();
    quad->op = QK_LABEL;
    quad->label = label;
}
Var* gen_expr(Node*node)
{
    switch (node->kind)
    {
    case NK_ADD:     //  +
        return gen_add(gen_expr(node->lhs),
                     gen_expr(node->rhs), node->tok);
    case NK_SUB:     //  -
        return gen_sub(gen_expr(node->lhs),
                    gen_expr(node->rhs), node->tok);
    case NK_DIV:     //  /
    case NK_MUL:     //  *
        return gen_mul_sub(node->kind, gen_expr(node->lhs),
                        gen_expr(node->rhs), node->tok);
    case NK_EQ:      // ==
    case NK_NE:      // !=
    case NK_LT:      // <  ### > ==> <
    case NK_LE:      // <=
        return gen_cmp(node->kind, gen_expr(node->lhs),
                         gen_expr(node->rhs), node->tok);
    case NK_NUM:     //  integer
    case NK_VAR:   // var
        return node->var;
    case NK_ASSIGN:  // =
        return gen_assign(gen_get_addr(node->lhs),
                         gen_expr(node->rhs), node->tok);
    case NK_ADDR:   //  &
        return gen_get_addr(node->lhs);
    case NK_DEREF:    //  *
        return gen_get_content(gen_expr(node->lhs), node->tok);
    case NK_FUNCTION://funtion
        return gen_func_call(node);
    default:
        return NULL;
    }
}
void gen_stmt(Node*node)
{
    switch (node->kind)
    {
    case NK_RETURN:  // "return"
    {
        Var *ret = gen_expr(node->lhs);
        if(!is_same(cur_func->type,ret->type))
            error_tok(node->tok, "return type is not same as function");
        Quad *quad = new_quad();
        quad->op = QK_RETURN;
        quad->lhs = ret;
        return;
    }
    case NK_IF:      //if
    {
        if (node->els)
        {
            int else_label = new_label(), end_label = new_label();
            Var * cond = gen_expr(node->cond);
            gen_jump(QK_JEZ, cond, NULL, else_label);
            gen_stmt(node->then);
            gen_jump(QK_JMP, NULL, NULL, end_label);
            gen_label(else_label);
            gen_stmt(node->els);
            gen_label(end_label);
        }
        else
        {
            int end_label = new_label();
            Var * cond = gen_expr(node->cond);
            gen_jump(QK_JEZ, cond, NULL, end_label);
            gen_stmt(node->then);
            gen_label(end_label);
        }
        return;
    }
    case NK_WHILE:   //while
    {
        int begin_label = new_label(), end_label = new_label();
        gen_label(begin_label);
        Var * cond = gen_expr(node->cond);
        gen_jump(QK_JEZ, cond, NULL, end_label);
        gen_stmt(node->then);
        gen_jump(QK_JMP, NULL, NULL, begin_label);
        gen_label(end_label);
        return;
    }
    case NK_FOR:     //for
    {
        Var *init = NULL, *cond = NULL, *inc = NULL;
        if (node->init)
            init = gen_expr(node->init);
        int begin_label = new_label(), end_label = new_label();
        gen_label(begin_label);

        if (node->cond)
        {
            cond = gen_expr(node->cond);
            gen_jump(QK_JEZ, cond, NULL, end_label);
        }
        gen_stmt(node->then);
        if (node->inc)
            inc = gen_expr(node->inc);
        gen_jump(QK_JMP, NULL, NULL, begin_label);
        gen_label(end_label);

        return;
    }
    case NK_BLOCK:   //block
    {
        for (Node *cur = node->body; cur; cur = cur->next)
            gen_stmt(cur);
        return;
    }
    default :
        gen_expr(node);
        return;
    }
}
QuadSet* gen_quadset(Function*func)
{
    quadset = calloc(1, sizeof(QuadSet));
    cur_func = func;
    for (Node *cur = func->node; cur; cur = cur->next)
    {
        gen_stmt(cur);
    }
    int tempsize=calc_temp_offset();
    quadset->temp_size = tempsize;
    quadset->local_size = func->local_size;
    quadset->name = func->name;
    quadset->params = func->params;

    return quadset;
}
QuadSet* gen_quadsets(Function*funclist)
{
    QuadSet qset;
    QuadSet *curset = &qset;
    for (Function *func = funclist; func;func=func->next)
    {
        curset->next = gen_quadset(func);
        curset = curset->next;
    }
    return qset.next;
}


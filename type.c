#include "wcc.h"
Type *int_type = &(Type){TY_INT, 4};
Type *long_type = &(Type){TY_LONG, 8};
Type *ptr_type = &(Type){TY_PTR, 8};



bool is_integer(Type *ty)
{
    return ty->kind == TY_INT;
}
bool is_array(Type*ty)
{
    return ty->kind == TY_ARRAY;
}
bool is_pointer(Type*ty)
{
    return ty->kind == TY_PTR ;
}
bool is_ptr_arr(Type*ty)
{
    return ty->kind == TY_PTR || ty->kind == TY_ARRAY;
}
bool is_same(Type*ty1,Type*ty2)
{
    if(ty1 == NULL && ty2 == NULL)
        return true;
    if (ty1 == NULL || ty2 == NULL)
        return false;
    if(ty1->kind != ty2->kind)
        return false;
    else
        return is_same(ty1->base,ty2->base);
}

bool is_equal(Type*ty1,Type*ty2)
{
    if(ty1 == NULL && ty2 == NULL)
        return true;
    if (ty1 == NULL || ty2 == NULL)
        return false;
    if(is_pointer(ty1) && is_pointer(ty2))
        return is_same(ty1->base, ty2->base);
    else if(ty1->kind!=ty2->kind)
        return false;
    else
        return true;
}

Type *point_to(Type *base)
{
    Type *type = calloc(1, sizeof(Type));
    type->base = base;
    type->kind = TY_PTR;
    type->size = 8;
}
Type *array_of(Type * base,int arrlen)
{
    Type * type  = calloc(1,sizeof(Type));
    type->base = base;
    type->kind = TY_ARRAY;
    type->arr_lens = arrlen;
    type->size = arrlen * base->size;
    return type;
}


#include "wcc.h"

#define print(fmt, ...) fprintf(codeout, fmt, ##__VA_ARGS__)
typedef enum
{
    RS_NULL,
    RS_NOTUSE, //表示寄存器不能使用，例如要保留一个寄存器以备后用的时候可以设置这个标志
    RS_CONST,  // if content is num
    RS_TMP,
    RS_LOCAL, // if content is local var
} RvalKind;
static const char *reg32[] = {
    "",
    "%eax",
    "%ebx",
    "%ecx",
    "%edx",
    "%esi",
    "%edi",
    "%r8d",
    "%r9d",
    "%r10d",
    "%r11d",
    "%r12d",
    "%r13d",
    "%r14d",
    "%r15d",
};
static const char *reg64[] = {
    "",
    "%rax",
    "%rbx",
    "%rcx",
    "%rdx",
    "%rsi",
    "%rdi",
    "%r8",
    "%r9",
    "%r10",
    "%r11",
    "%r12",
    "%r13",
    "%r14",
    "%r15",
};
static const char *arg_reg[] = {"","%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};

static const int reg_num = sizeof(reg32) / sizeof(reg32[0]);
static Var * rvalue[sizeof(reg32) / sizeof(reg32[0])];
#define ANY_REG -1
#define MEMORY  0

//目前采用LRU来管理寄存器。
//根据一个有关运算的四元式，根据这个四元式分配寄存器
//要注意到，op x,y 中x会被修改，并作为z(z=x op y)

static int get_size(Var *var) { return var->type->size; }

static int reg_index(const char * reg_name)
{
    for (int i = 1; i < reg_num;i++)
    {
        if(strcmp(reg_name,reg32[i])==0 || strcmp(reg_name,reg64[i])==0)
            return i;
    }
    return 0;
}
static const char * reg_name(int reg,int size)
{
    if(size==4)
        return reg32[reg];
    else
        return reg64[reg];
}

static bool in_reg(Var *x);
static int get_reg_index(Var *x);

static bool disable_list[sizeof(reg32) / sizeof(reg32[0])];
static void use(int reg) { disable_list[reg] = false; }
static void not_use(int reg) { disable_list[reg] = true; }

//判断一个寄存器内存储的值是否在内存中
static bool in_memory(int reg)
{
    if(rvalue[reg]==NULL)
        return true;
    if(rvalue[reg]->kind == VK_CONST)
        return true;
    else
        return rvalue[reg]->in_memory;
}
static void keep_fit(Var * var){var->in_memory = true;}
static void no_fit(Var *var) { var->in_memory = false; }


static void gen_mem2reg(Var*var,int reg)
{
    if(get_size(var)==4)
        print("\tmovl ");
    else
        print("\tmovq ");

    const char *regname = reg_name(reg, get_size(var));
    if (var->kind == VK_CONST)
        print("$%d,%s\n", var->val, regname);
    else 
        print("-%d(%%rbp),%s\n", var->offset, regname);
}
static void gen_reg2mem(int reg,Var*var)
{
    if(get_size(var)==4)
        print("\tmovl ");
    else
        print("\tmovq ");

    const char *regname = reg_name(reg, get_size(var));
    if (var->kind == VK_CONST)
        return;
    else
        print("%s,-%d(%%rbp)\n", regname, var->offset);
}

//清空reg，以及对应的变量常量等表
static void clear_reg(int reg)
{
    rvalue[reg] = NULL;
}
//将寄存器中的内容存储到内存，生成对应指令，注意不会清空reg，但会设置inmemory
//可能存在只想保存，但不想清空关系的情况
static void store(int reg)
{
    if(in_memory(reg))
        return;

    Var *var = rvalue[reg];
    print("\tmov%s %s,-%d(%%rbp)\n",get_size(var)==4?"l":"q",
          reg_name(reg, get_size(var)),
          var->offset);
    keep_fit(var);
}
static void store_if_islocal(int reg)
{
    if(rvalue[reg]->kind == VK_LOCAL)
        store(reg);
}
//设置reg以及对应的变量常量等表
static void set_reg(int reg,Var * var)
{
    rvalue[reg] = var;
}
//将内容从存储器中读入到reg中
static void load(Var * var, int reg)
{
    int w = get_size(var);
    const char *optype = w == 4 ? "l" : "q";
    print("\tmov%s ", optype);

    if(var->kind == VK_CONST)
        print("$%d", var->val);
    else
        print("-%d(%%rbp)", var->offset);
    print(",%s\n", reg_name(reg, get_size(var)));
}

//分配一个寄存器，如果寄存器不空闲则还需要清空寄存器

static int alloc_reg()
{
    for (int i = reg_num-1; i >=1 ; --i)
        if (rvalue[i]==NULL)
            return i;
    
    int idx;
    while (1)
    {
        idx = rand() % reg_num;
        if (idx == 0 || disable_list[idx] == true)
            continue;
        else
            break;
    }
    store(idx);
    clear_reg(idx);
    return idx;
}

static void store_all_var()
{
    for (int i = 1; i < reg_num;++i)
    {
        if(rvalue[i] && rvalue[i]->kind == VK_LOCAL)
        {
            store(i);
            clear_reg(i);
        }
    }
}


//将var保存到指定寄存器，并保存对应寄存器的状态
static void move_to_reg(Var * var,int reg)
{
    if(reg == ANY_REG)
    {
        if(in_reg(var))
            return;
        reg = alloc_reg();

    }

    int var_reg = get_reg_index(var);
    if(var_reg==reg)
        return;
    else if(var_reg!=0)
    {
        store(var_reg);
        clear_reg(var_reg);
        int w = get_size(var);
        print("\tmov%s %s,%s\n", (w == 4 ? "l" : "q"),
              reg_name(var_reg, w),
              reg_name(reg, w));
        set_reg(reg, var);
    }
    else 
    {
        store(reg);
        clear_reg(reg);
        load(var, reg);
        set_reg(reg, var);
    }
}
//将var保存到指定寄存器，并保存对应寄存器的状态
static void move_to_reg_np(Var * var,int reg)
{
    if(reg == ANY_REG)
    {
        if(in_reg(var))
            return;
        reg = alloc_reg();

    }

    int var_reg = get_reg_index(var);
    if(var_reg==reg)
        return;
    else if(var_reg!=0)
    {
        store(var_reg);
        clear_reg(var_reg);
        set_reg(reg, var);
    }
    else 
    {
        store(reg);
        clear_reg(reg);
        set_reg(reg, var);
    }
}

static void drop_if_istemp(int reg)
{
    if(reg==0)
        return;
    if(rvalue[reg]->kind == VK_TEMP)
        clear_reg(reg);
}
static void drop(int reg)
{
    clear_reg(reg);
}

//
/*
 *

    应该要考虑一下一个统一的方法 z=x op y  分为三类，变量表，临时变量表和常量表
    因为要产生这样的计算式子，为了防止x和y之间冲突，所以应该在一个函数内完成xy的寄存器配置

    首先是分配寄存器，首先考虑x在寄存器中：（则考虑使用该寄存器）
        如果x是常量，则直接分配
        如果x是临时变量，因为我们保证了临时变量只使用和定义一次，所以必然可以直接分配
        如果x是变量，则考虑是否与z相同，如果相同，则直接分配，否则需要生成保存指令(也不需要考虑)
    x如果不在寄存器中，则需要去任意分配寄存器

    为x任意分配一个寄存器的规则：
    （注意Z必然的是一个未被分配的，因为考虑抽象语法树，Z必然不是叶子节点，
    所以是临时节点，并且该句为定义，所以就必然此前未被分配过）
    如果有空寄存器则直接分配
    如果都满了，则首先分配存储常量的寄存器，并且不需要生成保存常量的语句
    否则则随机选择一个替换出去，并生成保存语句。

    然后生成y的寄存器，如果y在寄存器中则万事大吉。
    如果不在，则分配一个寄存器，注意不要与x起冲突，生成方式和x相同。


 *
 */
//如果x在reg中，生成寄存器名，如果在内存中，则生成地址
static int get_reg_index(Var *x)
{
    for (int i = 1; i < reg_num;++i)
        if(x == rvalue[i])
            return i;
    return 0;
}
static const char * get_reg_name(Var * x)
{
    int reg = get_reg_index(x);
    if(reg == 0)
        return NULL;
    return reg_name(reg, get_size(x));
}
static bool in_reg(Var * x)
{
    return get_reg_index(x) != 0;
}

void gen_location(Var * x)
{
    if(in_reg(x))
        print("%s", get_reg_name(x));
    else if(x->kind == VK_CONST)
        print("$%d",x->val);
    else
        print("-%d(%%rbp)", x->offset);
}

void gen_binary(const char * op,Var * x,Var * y)
{
    const char *optype = get_size(y) == 4 ? "l" : "q";
    print("\t%s%s ", op, optype);
    gen_location(x);
    print(",");
    gen_location(y);
    print("\n");
}

//生成2操作数的运算代码
// subl ax,bx  := bx = bx - ax
void gen_2operate_code(Quad *quad)
{
    const char *op = NULL;
    switch (quad->op)
    {
    case QK_ADD:
        op = "add";
        break;
    case QK_SUB:
        op = "sub";
        break;
    case QK_MUL:
        op = "imul";
        break;
    default:
        break;
    }

    move_to_reg(quad->lhs, ANY_REG);
    gen_binary(op, quad->rhs, quad->lhs);

    int l_reg = get_reg_index(quad->lhs), r_reg = get_reg_index(quad->rhs);
    drop(l_reg), drop(r_reg);
    set_reg(l_reg, quad->result);
}

//将比较结果统一放在eax中,因此需要对eax进行清空处理等
void gen_cmp_code(Quad *quad)
{
    int eax = reg_index("%eax");

    if(!in_reg(quad->lhs) && !in_reg(quad->rhs))
        move_to_reg(quad->lhs, ANY_REG);
    int r_reg = get_reg_index(quad->rhs),l_reg=get_reg_index(quad->lhs);

    gen_binary("cmp", quad->rhs, quad->lhs);

    drop(l_reg), drop(r_reg);

    //然后清空eax，为结果留下空间
    store(eax);    
    clear_reg(eax);
    set_reg(eax,quad->result);

    switch (quad->op)
    {
    case QK_EQ:
        print("\tsete %%al\n");
        break;
    case QK_NE:
        print("\tsetne %%al\n");
        break;
    case QK_LE:
        print("\tsetle %%al\n");
        break;
    case QK_LT:
        print("\tsetl %%al\n");
        break;
    default:
        break;
    }
    print("\tmovsbl %%al,%%eax\n");
    
}

//由于除法指令需要保证[edx;eax]为被除数，所以没有和上文统一格式，因而单独生成
// [edx;eax] / y  -> 商：%eax  余数： %edx
//所以这里会进行一系列整除和商的运算
void gen_div_code(Quad *quad)
{
    int eax = reg_index("%eax"), edx = reg_index("%edx");

    store(eax), clear_reg(eax);
    store(edx), clear_reg(edx);

    not_use(eax), not_use(edx);

    move_to_reg(quad->lhs, eax);
    move_to_reg(quad->rhs, ANY_REG);

    int w = get_size(quad->lhs);
    print("\t%s\n", w == 4 ? "cltd" : "cqto");
    print("\tidiv%s %s\n",w==4?"l":"q", get_reg_name(quad->rhs));
    set_reg(eax,quad->result);

    use(eax), use(edx);

    drop(get_reg_index(quad->lhs));
    drop(get_reg_index(quad->rhs));
}

/*
return生成思路：因为需要保存在eax中，所以需要将表达式的最终结果移动到eax中，
所以首先判断表达式是否已经在eax中，如果没有，则需要将eax清空，
然后将表达式移动或者加载到eax中*/
void gen_return_code(Quad *quad)
{
    int eax = reg_index("%eax");
    move_to_reg(quad->lhs, eax);
    print("\tjmp .L.return.%s\n", quadset->name);
    drop(eax);
}


// 统一，右边部分是地址，左边部分是值，因而是对内存地址直接赋值
void gen_assign_code(Quad *quad)
{
    move_to_reg(quad->lhs, ANY_REG);
    int sreg = get_reg_index(quad->lhs);
    not_use(sreg);
    move_to_reg(quad->result, ANY_REG);
    use(sreg);
    int treg = get_reg_index(quad->result);

    int w = get_size(quad->lhs);
    print("\tmov%s %s,(%s)\n", w == 4 ? "l" : "q", get_reg_name(quad->lhs),
          get_reg_name(quad->result));
    
    drop(sreg);
}

void gen_jump_code(Quad*quad)
{
    switch (quad->op)
    {
    case QK_JEZ:
    {
        move_to_reg(quad->lhs, ANY_REG);
        gen_binary("cmp", new_iconst(0), quad->lhs);
        drop(get_reg_index(quad->lhs));
        print("\tje .L%d\n", quad->label);
        break;
    }
    case QK_JMP:
        print("\tjmp .L%d\n", quad->label);
        break;
    default:
        break;
    }
}
void gen_call_code(Quad * quad)
{
    for (int i = 1; i < reg_num;++i)
    {
        store(i);
        clear_reg(i);
    }
    int eax = reg_index("%eax");
    print("\tcall %s\n", quad->func_name);
    //记得返回值和func绑定，func节点拥有一个temp位置
    set_reg(eax,quad->result);
}
void gen_deref(Quad * quad)
{
    int w = get_size(quad->result);
    move_to_reg(quad->lhs, ANY_REG);
    int s_reg = get_reg_index(quad->lhs);

    print("\tmov%s (%s),%s\n", w == 4 ? "l" : "q",
              get_reg_name(quad->lhs),
              reg_name(s_reg,w));
              
    set_reg(s_reg, quad->result);
}
static void gen_get_addr(Quad * quad)
{
    move_to_reg_np(quad->result, ANY_REG);
    print("\tleaq -%d(%%rbp),%s\n", quad->lhs->offset, get_reg_name(quad->result));
}
void gen_int2long(Quad * quad)
{
    move_to_reg(quad->lhs,ANY_REG);
    int reg = get_reg_index(quad->lhs);
    print("\tmovslq %s,%s\n", reg32[reg], reg64[reg]);
    
    set_reg(reg, quad->result);
}

//初始化函数参数，将其与寄存器关联起来
void init_func_params()
{
    int param_index = 1;
    for (VarList *params = quadset->params; params;params = params->next)
    {
        Var *param = params->var;
        //将其与寄存器关联起来
        int reg = reg_index(arg_reg[param_index]);
        set_reg(reg, param);
        store(reg);
        clear_reg(reg);
        param_index++;
    }
}
void gen_funcion()
{
    print(".global %s\n",quadset->name);
    print("%s:\n",quadset->name);

    print("\tpushq %%rbp\n");
    print("\tmovq %%rsp, %%rbp\n");
    int size_of_all = quadset->local_size + quadset->temp_size;
    //16对齐
    size_of_all = (size_of_all+15) / 16 * 16 ;
    print("\tsubq $%d,%%rsp\n",size_of_all);

    init_func_params();
    for (int i = 0; i < quadset->size; ++i)
    {
        print("\n");
        Quad *quad = &quadset->list[i];
        switch (quad->op)
        {
        //对于二元操作z:=x op y 由于一次定义一次引用，所以x如果在reg中则可以作为z的保存
        //如果在内存中，则需要一次加载，这个时候分配一个寄存器即可。
        case QK_ADD:
        case QK_SUB:
        case QK_MUL:
            gen_2operate_code(quad);
            break;
        case QK_DIV:
            gen_div_code(quad);
            break;
        // for cmp oper
        case QK_LE:
        case QK_EQ:
        case QK_LT:
        case QK_NE:
            gen_cmp_code(quad);
            break;
        case QK_RETURN:
            gen_return_code(quad);
            break;
        case QK_ASSIGN:
            gen_assign_code(quad);
            break;
        case QK_JMP:
        case QK_JEZ:
            gen_jump_code(quad);
            break;
        case QK_LABEL:
            print(".L%d:\n", quad->label);
            break;
        case QK_PARAM:
            move_to_reg(quad->lhs, reg_index(arg_reg[quad->param_no]));
            break;
        case QK_CALL:
            gen_call_code(quad);
            break;
        case QK_DEREF:
            gen_deref(quad);
            break;
        case QK_ADDR:
            gen_get_addr(quad);
            break;
        case QK_INT2LONG:
            gen_int2long(quad);
        default:
            break;
        }
    }

    // Epilogue
    
    print(".L.return.%s:\n", quadset->name);
    print("\tleave\n");
    print("\tret\n");

    //清空所有的reg
    for (int i = 1; i < reg_num;++i)
        clear_reg(i);
}
void gen_code()
{
    while(quadset)
    {
        gen_funcion();
        quadset = quadset->next;
    }
}
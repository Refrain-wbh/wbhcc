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
typedef struct Rvalue Rvalue;

struct Rvalue
{
    RvalKind kind;
    ConstVal *constval;
    Temp *temp;
    Var *local;
};
static char *regname[] = {
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
static char *argreg[] = {"%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};
static int eax = 1, edx = 4;

static int regnum = sizeof(regname) / sizeof(regname[0]);
static Rvalue rvalue[sizeof(regname) / sizeof(regname[0])];


//目前采用LRU来管理寄存器。
//根据一个有关运算的四元式，根据这个四元式分配寄存器
//要注意到，op x,y 中x会被修改，并作为z(z=x op y)

static int regidx(char *name)
{
    for (int i = 1; i < regnum; ++i)
        if (strcmp(name, regname[i]) == 0)
            return i;
    return 0;
}
static int get_reg(Node *node)
{
    int regstate = 0;
    if (node->kind == NK_NUM) // is const
        regstate = node->constval->reg;
    else if (node->kind == NK_IDENT)
        regstate = node->var->reg;
    else
        regstate = node->temp->reg;
    return regstate;
}
//判断一个节点是否在寄存器中
static bool inreg(Node *node)
{
    return get_reg(node) != 0;
}
//判断一个寄存器内存储的值是否在内存中
static bool inmemory(int reg)
{
    if (rvalue[reg].kind == RS_LOCAL)
        return rvalue[reg].local->inmemory;
    if (rvalue[reg].kind == RS_TMP)
        return rvalue[reg].temp->inmemory;
    return true;
}
//清空reg，以及对应的变量常量等表
static void clear_reg(int reg)
{
    if (rvalue[reg].kind == RS_TMP)
        rvalue[reg].temp->reg = 0;
    else if (rvalue[reg].kind == RS_CONST)
        rvalue[reg].constval->reg = 0;
    else if (rvalue[reg].kind == RS_LOCAL)
        rvalue[reg].local->reg = 0;

    rvalue[reg].kind = RS_NULL;
}
//将寄存器中的内容存储到内存，生成对应指令，注意不会清空reg，但会设置inmemory
static void store(int reg)
{
    if (rvalue[reg].kind == RS_TMP && rvalue[reg].temp->inmemory == 0)
    {
        int temp_offset = rvalue[reg].temp->offset;
        print("\tmovl %s,-%d(%%rbp)\n", regname[reg], quadset->local_size + temp_offset);
        rvalue[reg].temp->inmemory = 1;
    }

    if (rvalue[reg].kind == RS_LOCAL && rvalue[reg].local->inmemory == 0)
    {
        int local_offset = rvalue[reg].local->offset;
        print("\tmovl %s,-%d(%%rbp)\n", regname[reg], local_offset);
        rvalue[reg].local->inmemory = 1;
    }
}
//设置reg以及对应的变量常量等表
static void set_reg(Node *node, int reg)
{
    clear_reg(reg);
    if (node->kind == NK_NUM)
    {
        rvalue[reg].kind = RS_CONST;
        rvalue[reg].constval = node->constval;
        node->constval->reg = reg;
    }
    else if (node->kind == NK_IDENT)
    {
        rvalue[reg].kind = RS_LOCAL;
        rvalue[reg].local = node->var;
        node->var->reg = reg;
    }
    else
    {
        rvalue[reg].kind = RS_TMP;
        rvalue[reg].temp = node->temp;
        node->temp->reg = reg;
    }
}
//将内容从存储器中读入到reg中
static void load(Node *node, int reg)
{
    if (node->kind == NK_NUM)
    {
        print("\tmovl $%d,%s\n", node->constval->val, regname[reg]);
    }
    else if (node->kind == NK_IDENT)
    {
        int local_offset = node->var->offset;
        print("\tmovl -%d(%%rbp),%s\n", local_offset, regname[reg]);
    }
    else
    {
        int temp_offset = node->temp->offset;
        print("\tmovl -%d(%%rbp),%s\n", quadset->local_size + temp_offset, regname[reg]);
    }
}
//分配一个寄存器，如果寄存器不空闲则还需要清空寄存器
static int alloc_reg()
{
    for (int i = regnum-1; i >=1 ; --i)
    {
        if (rvalue[i].kind == RS_NULL)
            return i;
    }
    int idx;
    while (1)
    {
        idx = rand() % regnum;
        if (idx == 0 || rvalue[idx].kind == RS_NOTUSE)
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
    for (int i = 1; i < regnum;++i)
    {
        if(rvalue[i].kind == RS_LOCAL)
        {
            store(i);
            clear_reg(i);
        }
    }
}
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
//生成2操作数的运算代码
void gen_2operate_code(Quad *quad)
{
    int regx = 0, regy;
    if (!inreg(quad->arg1))
    {
        regx = alloc_reg();
        rvalue[regx].kind = RS_NOTUSE;
    }
    if (!inreg(quad->arg2))
    {
        regy = alloc_reg();
        set_reg(quad->arg2, regy);
        load(quad->arg2, regy);
    }
    if (regx)
    {
        set_reg(quad->arg1, regx);
        load(quad->arg1, regx);
    }
    // from now  x y reg is set sucess both cotain the right val
    regx = get_reg(quad->arg1), regy = get_reg(quad->arg2);
    if (rvalue[regx].kind == RS_LOCAL)
        store(regx);
    clear_reg(regx);

    //如果是第二个操作数是临时变量，则可以直接丢弃
    if (rvalue[regy].kind == RS_TMP)
        clear_reg(regy);

    switch (quad->op)
    {
    case QK_ADD:
        print("\taddl %s,%s\n", regname[regy], regname[regx]);
        break;
    case QK_SUB:
        print("\tsubl %s,%s\n", regname[regy], regname[regx]);
        break;
    case QK_MUL:
        print("\timull %s,%s\n", regname[regy], regname[regx]);
        break;
    default:
        break;
    }

    //设置结果保存的位置 由于生成的必然是临时变量，故可以不保存
    set_reg(quad->result, regx);
}

//将比较结果统一放在eax中,因此需要对eax进行清空处理等
void gen_cmp_code(Quad *quad)
{
    int regx = 0, regy;
    if (!inreg(quad->arg1))
    {
        regx = alloc_reg();
        rvalue[regx].kind = RS_NOTUSE;
    }
    if (!inreg(quad->arg2))
    {
        regy = alloc_reg();
        set_reg(quad->arg2, regy);
        load(quad->arg2, regy);
    }
    if (regx)
    {
        set_reg(quad->arg1, regx);
        load(quad->arg1, regx);
    }

    // from now  x y reg is set sucess
    regx = get_reg(quad->arg1), regy = get_reg(quad->arg2);
    print("\tcmpl %s,%s\n", regname[regy], regname[regx]);

    if (rvalue[regx].kind == RS_TMP)
        clear_reg(regx);
    if (rvalue[regy].kind == RS_TMP)
        clear_reg(regy);
    //然后清空eax，为结果留下空间
    store(eax);
    clear_reg(eax);
    set_reg(quad->result, eax);

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
    //首先清空edx，并生成相应的保存语句
    if (rvalue[edx].kind != RS_NULL)
    {
        store(edx);
        clear_reg(edx);
    }
    rvalue[edx].kind = RS_NOTUSE;

    //然后获取arg1的reg，如果不是eax，那么就要先保存eax，然后将奥arg1加载到eax中
    //注意div会让eax发生改变，但由于临时变量一次定义一次使用，所以无需担心
    int xreg = get_reg(quad->arg1);
    if (xreg != eax)
    {
        if (rvalue[eax].kind != RS_NULL)
            store(eax);

        if (xreg == 0)
        {
            set_reg(quad->arg1, eax);
            load(quad->arg1, eax);
        }
        else
        {
            clear_reg(xreg);
            set_reg(quad->arg1, eax);
            print("\tmovl %s,%%eax\n", regname[xreg]);
        }
    }

    //加载y
    if (!inreg(quad->arg2))
    {
        RvalKind tmpkind = rvalue[eax].kind;
        rvalue[eax].kind = RS_NOTUSE;

        int yreg = alloc_reg();
        set_reg(quad->arg2, yreg);
        load(quad->arg2, yreg);

        rvalue[eax].kind = tmpkind;
    }

    print("\tcltd\n");
    print("\tidivl %s\n", regname[get_reg(quad->arg2)]);

    if (quad->op == QK_DIV)
        set_reg(quad->result, eax);

    //如果除数为临时变量，则直接丢弃
    int regy = get_reg(quad->arg2);
    if (rvalue[regy].kind == RS_TMP)
        clear_reg(regy);
}

/*
return生成思路：因为需要保存在eax中，所以需要将表达式的最终结果移动到eax中，
所以首先判断表达式是否已经在eax中，如果没有，则需要将eax清空，
然后将表达式移动或者加载到eax中*/
void gen_return_code(Quad *quad)
{
    Node *ret = quad->arg1;
    int reg = get_reg(ret);
    if (eax != reg)
    {
        store(eax);
        clear_reg(eax);
        if (inreg(ret))
        {
            print("\tmovl %s,%%eax\n", regname[reg]);
            clear_reg(reg);
        }
        else
        {
            load(ret, eax);
            //加载之后立刻就失效了，所以不需要做操作
        }
    }
    print("\tjmp .L.return.%s\n", quadset->name);
}

void gen_assign_code(Quad *quad)
{
    Node *source = quad->arg1, *target = quad->result;
    // target 必定是indentity 因而此时有local发生修改，注意修改inmemory的标志
    int sreg = 0, treg = 0;
    if (!inreg(source))
    {
        sreg = alloc_reg();
        rvalue[sreg].kind = RS_NOTUSE;
    }
    if (!inreg(target))
    {
        treg = alloc_reg();
        set_reg(target, treg);
    }
    if (sreg != 0)
    {
        load(source, sreg);
        set_reg(source, sreg);
    }
    sreg = get_reg(source), treg = get_reg(target);

    print("\tmovl %s,%s\n", regname[sreg], regname[treg]);

    // target 内容已经发生改变，需要更新inmemory
    rvalue[treg].local->inmemory = 0;
    if (rvalue[sreg].kind == RS_CONST)
        clear_reg(sreg);
}

void gen_jump_code(Quad*quad)
{

    switch (quad->op)
    {
    case QK_JEZ:
    {
        int reg;
        if(!inreg(quad->arg1))
        {
            reg = alloc_reg();
            load(quad->arg1,reg);
            set_reg(quad->arg1, reg);
        }
        reg = get_reg(quad->arg1);
        print("\tcmpl $0,%s\n", regname[reg]);
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
    //权且采用一种简单的方式，如果是变量或者是临时变量都保存下俩
    for (int reg = 1; reg < regnum;++reg)
    {
        store(reg);
        clear_reg(reg);
    }
    Node *func = quad->result;
    int argno=0;
    for (Node *argnode = func->args; argnode;argnode=argnode->next)
    {
        int reg = regidx(argreg[argno]);
        load(argnode, reg);
        set_reg(argnode,reg);
        argno++;
        if (argno >= 6)
            break;
    }
    print("\tcall %s\n", func->funcname);
    //记得返回值和func绑定，func节点拥有一个temp位置
    set_reg(func, eax);
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

    for (int i = 0; i < quadset->size; ++i)
    {
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
            store_all_var();
            gen_jump_code(quad);
            break;
        case QK_LABEL:
            store_all_var();
            print(".L%d:\n", quad->label);
            break;
        case QK_CALL:
            gen_call_code(quad);
            break;
        default:
            break;
        }
    }

    // Epilogue
    
    print(".L.return.%s:\n", quadset->name);
    print("\tleave\n");
    print("\tret\n");
}
void gen_code()
{
#ifdef DEBUG 
    assert(strcmp(regname[edx], "%edx") == 0);
    assert(strcmp(regname[eax], "%eax") == 0);
#endif

    while(quadset)
    {
        gen_funcion();
        quadset = quadset->next;
    }
}
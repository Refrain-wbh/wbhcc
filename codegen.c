#include "wcc.h"


#define print(fmt,...) fprintf(codeout,fmt,##__VA_ARGS__)
typedef enum
{
    RS_NULL,
    RS_NOTUSE, //表示寄存器不能使用，例如要保留一个寄存器以备后用的时候可以设置这个标志
    RS_CONST,
    RS_TMP,
    RS_VAL, // if content is num
}RvalKind;
typedef struct Rvalue Rvalue;


struct Rvalue
{
    RvalKind kind;
    Constval*constval;
    Temp *temp;
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

static int regnum = sizeof(regname) / sizeof(regname[0]);
static Rvalue rvalue[sizeof(regname) / sizeof(regname[0])];



//目前采用LRU来管理寄存器。
//根据一个有关运算的四元式，根据这个四元式分配寄存器
//要注意到，op x,y 中x会被修改，并作为z(z=x op y)


static int regidx(char *name)
{
    for (int i = 1; i < regnum;++i)
        if(strcmp(name,regname[i])==0)
            return i;
    return 0;
}
static int get_reg(Node*node)
{
    int regstate = 0;
    if (node->kind == NK_NUM)//is const var
        regstate = node->constval->reg;
    else
        regstate = node->temp->reg;
    return regstate;
}
//判断一个节点是否在寄存器中
static bool inreg(Node * node)
{
    return get_reg(node) != 0;
}


//清空reg，以及对应的变量常量等表
static void clear_reg(int reg)
{
    if(rvalue[reg].kind==RS_TMP)
        rvalue[reg].temp->reg = 0;
    else if(rvalue[reg].kind==RS_CONST)
        rvalue[reg].constval->reg = 0;

    rvalue[reg].kind = RS_NULL;
}
//将寄存器中的内容存储到内存，生成对应指令，注意不会清空reg，但会设置inmemory
static void store(int reg)
{
    if(rvalue[reg].kind==RS_TMP)
    {
        print("\tmovl %s,",regname[reg]);
        print_tempaddr(codeout, rvalue[reg].temp);
        print("\n");
        rvalue[reg].temp->inmemory = 1;
    }
}
//设置reg以及对应的变量常量等表
static void set_reg(Node*node,int reg)
{
    clear_reg(reg);
    if (node->kind == NK_NUM)
    {
        rvalue[reg].kind = RS_CONST;
        rvalue[reg].constval = node->constval;
        node->constval->reg = reg;
    }
    else
    {
        rvalue[reg].kind = RS_TMP;
        rvalue[reg].temp = node->temp;
        node->temp->reg = reg;
    }
}
//将内容从存储器中读入到reg中
static void load(Node*node,int reg)
{
    if(node->kind==NK_NUM)
    {
        print("\tmovl $%d,%s\n", node->constval->val, regname[reg]);
    }
    else 
    {
        print("\tmovl ");
        print_tempaddr(codeout, node->temp);
        print(",%s\n", regname[reg]);
    }
}
//分配一个寄存器，如果寄存器不空闲则还需要清空寄存器
static int alloc_reg()
{
    for (int i = 1; i < regnum;++i)
    {
        if(rvalue[i].kind==RS_NULL)
            return i;
    }
    int idx;
    while (1)
    {
        idx = rand() % regnum;
        if(idx==0||rvalue[idx].kind==RS_NOTUSE)
            continue;
        else
            break;
    }
    store(idx);
    clear_reg(idx);
    return idx;
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
void gen_2operate_code(Quad*quad)
{

    int regx = 0,regy;
    if (!inreg(quad->arg1))
    {
        regx = alloc_reg();
        rvalue[regx].kind = RS_NOTUSE;
    }
    if(!inreg(quad->arg2))
    {
        regy = alloc_reg();
        set_reg(quad->arg2, regy);
        load(quad->arg2, regy);
    }
    if(regx)
    {
        set_reg(quad->arg1, regx);
        load(quad->arg1, regx);
    }

    //from now  x y reg is set sucess
    regx = get_reg(quad->arg1), regy = get_reg(quad->arg2);
    clear_reg(regx);
    set_reg(quad->result, regx);

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
}

//将比较结果统一放在eax中,因此需要对eax进行清空处理等
void gen_cmp_code(Quad*quad)
{
    const int eax = 1;
#ifdef DEBUG
    assert(strcmp(regname[eax], "%eax") == 0);
#endif
    int regx = 0,regy;
    if (!inreg(quad->arg1))
    {
        regx = alloc_reg();
        rvalue[regx].kind = RS_NOTUSE;
    }
    if(!inreg(quad->arg2))
    {
        regy = alloc_reg();
        set_reg(quad->arg2, regy);
        load(quad->arg2, regy);
    }
    if(regx)
    {
        set_reg(quad->arg1, regx);
        load(quad->arg1, regx);
    }

    //from now  x y reg is set sucess
    regx = get_reg(quad->arg1), regy = get_reg(quad->arg2);
    print("\tcmpl %s,%s\n", regname[regy],regname[regx]);
    
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
    print("\tmovzbl %%al,%%eax\n");

}

//由于除法指令需要保证[edx;eax]为被除数，所以没有和上文统一格式，因而单独生成
// [edx;eax] / y  -> 商：%eax  余数： %edx
//所以这里会进行一系列整除和商的运算
void gen_div_code(Quad*quad)
{
    const int edx = 4, eax = 1;
#ifdef DEBUG
    assert(strcmp(regname[edx], "%edx") == 0);
    assert(strcmp(regname[eax], "%eax") == 0);
#endif
    //首先清空edx，并生成相应的保存语句
    if(rvalue[edx].kind!=RS_NULL)
    {
        store(edx);
        clear_reg(edx);
    }
    rvalue[edx].kind = RS_NOTUSE;

    //然后获取arg1的reg，如果不是eax，那么就要先保存eax，然后将奥arg1加载到eax中
    //注意div会让eax发生改变，但由于临时变量一次定义一次使用，所以无需担心
    int xreg = get_reg(quad->arg1);
    if(xreg!=eax)
    {
        if(rvalue[eax].kind!=RS_NULL)
            store(eax);
        
        if(xreg==0)
        {
            set_reg(quad->arg1,eax);
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
    if(!inreg(quad->arg2))
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

    if(quad->op==QK_DIV)
        set_reg(quad->result, eax);
}

void gen_code()
{

    print(".global main\n");
    print("main:\n");

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
        //for cmp oper
        case QK_LE:
        case QK_EQ:
        case QK_LT:
        case QK_NE:
            gen_cmp_code(quad);
            break;
        default:
            break;
        }
    }

    Quad *quad = &quadset->list[quadset->size - 1];
    print("\tmovl %s,%%eax\n", regname[get_reg(quad->result)]);
    print("\tret\n");
}
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
static const char *regname[] = {
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
static const char *regname64[] = {
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
static char *argreg[] = {"%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};
static int eax = 1, edx = 4;

static int regnum = sizeof(regname) / sizeof(regname[0]);
static Rvalue rvalue[sizeof(regname) / sizeof(regname[0])];
#define ANY_REG -1
#define MEMORY  0

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
    else if (node->kind == NK_VAR)
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
//可能存在只想保存，但不想清空关系的情况
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
static void store_if_islocal(int reg)
{
    if(rvalue[reg].kind == RS_LOCAL)
        store(reg);
}
//设置reg以及对应的变量常量等表
static void set_reg(Node *node, int reg)
{
    clear_reg(reg);
    int nodereg = get_reg(node);
    clear_reg(nodereg);
    if (node->kind == NK_NUM)
    {
        rvalue[reg].kind = RS_CONST;
        rvalue[reg].constval = node->constval;
        node->constval->reg = reg;
    }
    else if (node->kind == NK_VAR)
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
    else if (node->kind == NK_VAR)
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
//将地址读入reg中
static void get_addr(Node*node,int reg)
{
    if(node->kind == NK_VAR)
    {
        int local_offset = node->var->offset;
        print("\tleaq -%d(%%rbp),%s\n", local_offset, regname64[reg]);
        return;
    }
    error_tok(node->tok, "expected an var");
}
static void get_content(int addr_reg,int tar_reg)
{
    store_all_var();
    print("\tmovl (%s),%s\n", regname64[addr_reg], regname[tar_reg]);
}
/*
将arg1移动到reg1中 包括如此情况：
两者均为有内容的，那么则表示将arg1移动到reg1中，如果arg1中本来就在另一个寄存器中，则
表示在寄存器之间移动，如果不在寄存器中，则表示加载
如果arg1为NULL，则表示将reg1清空
如果reg1为空，则表示将arg1清空
如果reg1为-1，则表示将arg1分配到任何一个寄存器中，如果在寄存器中，则不分配
*/

static void move_node(Node*node,int treg)
{
    if(node)
    {
        int reg = get_reg(node);
        if(reg == treg)
            return;
        
        if(treg == MEMORY)//则reg必然>0
        {
            store(reg);
            clear_reg(reg);
        }
        else if(treg==ANY_REG)    
        {
            if(reg > 0)
                return;
            //否则表示，在内存中，要加载到任何一个寄存器中
            reg = alloc_reg();
            load(node, reg);
            set_reg(node, reg);
        }
        else//有特定目标reg
        {
            store(treg);
            clear_reg(treg);
            if(reg>0)   //表示在不同reg之间切换
            {
                print("\tmovl %s,%s\n", regname[reg], regname[treg]);
                clear_reg(reg);
                set_reg(node, treg);
            }
            else 
            {
                load(node, treg);
                set_reg(node, treg);
            }
        }
    }
    else //表示清空treg
    {
        store(treg);
        clear_reg(treg);
    }
}

static void move_2node(Node*arg1,int reg1,Node*arg2,int reg2)
{
    move_node(arg1,reg1);
    int temp=rvalue[reg1].kind;
    rvalue[reg1].kind = RS_NOTUSE;
    move_node(arg2, reg2);
    rvalue[reg1].kind = temp;
}
static void drop_if_istemp(int reg)
{
    if(rvalue[reg].kind == RS_TMP)
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
//生成2操作数的运算代码
void gen_2operate_code(Quad *quad)
{
    move_2node(quad->arg1, ANY_REG, quad->arg2, ANY_REG);
    // from now  x y reg is set sucess both cotain the right val
    
    int regx = get_reg(quad->arg1), regy = get_reg(quad->arg2);
    
    //修改寄存器状态，设置结果保存的位置 由于生成的必然是临时变量，故可以不保存
    if (rvalue[regx].kind == RS_LOCAL)
        store(regx);
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

    set_reg(quad->result, regx);
    //如果是第二个操作数是临时变量，则可以直接丢弃
    drop_if_istemp(regy);
}

//将比较结果统一放在eax中,因此需要对eax进行清空处理等
void gen_cmp_code(Quad *quad)
{
    move_2node(quad->arg1, ANY_REG, quad->arg2, ANY_REG);
    // from now  x y reg is set sucess
    int regx = get_reg(quad->arg1), regy = get_reg(quad->arg2);
    //clear eax for store
    store(eax);
    clear_reg(eax);

    print("\tcmpl %s,%s\n", regname[regy], regname[regx]);
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

    
    drop_if_istemp(regx);
    drop_if_istemp(regy);
    //然后清空eax，为结果留下空间
    set_reg(quad->result, eax);
}

//由于除法指令需要保证[edx;eax]为被除数，所以没有和上文统一格式，因而单独生成
// [edx;eax] / y  -> 商：%eax  余数： %edx
//所以这里会进行一系列整除和商的运算
void gen_div_code(Quad *quad)
{
    move_2node(quad->arg1, eax, NULL, edx);
    int eaxrs = rvalue[eax].kind, edxrs = rvalue[edx].kind;
    rvalue[eax].kind = RS_NOTUSE, rvalue[edx].kind = RS_NOTUSE;
    move_node(quad->arg2, ANY_REG);
    rvalue[eax].kind = eaxrs, rvalue[edx].kind = edxrs;
    
    int regy = get_reg(quad->arg2);

    if(rvalue[eax].kind == RS_LOCAL)
        store(eax);

    print("\tcltd\n");
    print("\tidivl %s\n", regname[regy]);


    if (quad->op == QK_DIV)
        set_reg(quad->result, eax);

    //如果除数为临时变量，则直接丢弃
    drop_if_istemp(regy);
}

/*
return生成思路：因为需要保存在eax中，所以需要将表达式的最终结果移动到eax中，
所以首先判断表达式是否已经在eax中，如果没有，则需要将eax清空，
然后将表达式移动或者加载到eax中*/
void gen_return_code(Quad *quad)
{
    move_node(quad->arg1, eax);
    print("\tjmp .L.return.%s\n", quadset->name);
    drop_if_istemp(eax);
}

void gen_assign_code(Quad *quad)
{
    Node *source = quad->arg1, *target = quad->result;
    // target 必定是indentity 因而此时有local发生修改，注意修改inmemory的标志
    move_node(source, ANY_REG);
    int sreg = get_reg(source);
    set_reg(quad->result, sreg);
    int treg = sreg;
    // target 内容已经发生改变，需要更新inmemory
    rvalue[treg].local->inmemory = 0;
}

void gen_jump_code(Quad*quad)
{

    switch (quad->op)
    {
    case QK_JEZ:
    {
        move_node(quad->arg1, ANY_REG);
        int reg = get_reg(quad->arg1);
        clear_reg(reg);
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
    //权且采用一种简单的方式，如果是变量或者是临时变量都保存下来
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
void gen_get_addr_code(Quad*quad)
{

}
//初始化函数参数，将其与寄存器关联起来
void init_func_params()
{
    int paramidx = 0;
    for (VarList *params = quadset->params; params;params = params->next)
    {
        Var *param = params->var;
        //将其与寄存器关联起来
        int reg = regidx(argreg[paramidx]);
        rvalue[reg].kind = RS_LOCAL;
        rvalue[reg].local = param;
        param->reg = reg;
        paramidx++;
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
        case QK_DEREF:
        {
            if(!inreg(quad->result))
            {
                set_reg(quad->result, alloc_reg());
            }
            move_node(quad->arg1, ANY_REG);
            int treg = get_reg(quad->result), sreg = get_reg(quad->arg1);
            get_content(sreg,treg);
            drop_if_istemp(sreg);
            break;
        }
        case QK_ADDR:
        {
            if(!inreg(quad->result))
            {
                int reg = alloc_reg();
                set_reg(quad->result, reg);
            }
            int reg = get_reg(quad->result);
            get_addr(quad->arg1, reg);
            break;
        }
        case QK_PTR_ADD:
        case QK_PTR_SUB:
        {
            Node*arg1=quad->arg1,*arg2=quad->arg2;
            if(is_integer(arg1->kind))
            {
                Node *t = arg1;
                arg1 = arg2;
                arg2 = t;
            }
            
            move_2node(arg1, ANY_REG, arg2, ANY_REG);
            int regx = get_reg(arg1), regy = get_reg(arg2);
            store_if_islocal(regy);
            print("\timul %d,%s",)
            break;
        }

        default:
            break;
        }
    }

    // Epilogue
    
    print(".L.return.%s:\n", quadset->name);
    print("\tleave\n");
    print("\tret\n");

    //清空所有的reg
    for (int i = 1; i < regnum;++i)
        clear_reg(i);
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
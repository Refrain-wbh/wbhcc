#include "wcc.h"


#define print(fmt,...) fprintf(codeout,fmt,##__VA_ARGS__)
typedef enum
{
    RS_NULL,
    RS_NOTUSE,
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



//Ŀǰ����LRU������Ĵ�����
//����һ���й��������Ԫʽ�����������Ԫʽ����Ĵ���
//Ҫע�⵽��op x,y ��x�ᱻ�޸ģ�����Ϊz(z=x op y)

/*
char* getReg(Quad*quad)
{

    // �ٶ� z:=x op y ��x��ri�У�����z��x��ͬһ�����ţ���ѡ��riΪ�Ĵ���
    char *xreg = get_reg(quad->arg1), *zreg = get_reg(quad->result);
    if(xreg!=NULL&&xreg==zreg)
        return xreg;
    //�����û�з���ļĴ�������ѡ��һ��ri��Ϊ���
    int emptyreg = -1;
    for (int i = 0; i < regnum; ++i)
        if(rvalue[i].kind==RK_NULL)
        {
            emptyreg = i;
            break;
        }
    if(emptyreg!=-1)
        return regname + emptyreg;
    //���û�п��еļĴ���������LRU�ķ�ʽ��ѡ��Ĵ���ri

    //�������ri�еı������������A�����ߣ�M��A��C��������B��B����ri�У�
    static chose = 0;
    chose = (chose + 1) % regnum;

}
*/


static int get_reg(Node*node)
{
    int regstate = 0;
    if (node->kind == NK_NUM)//is const var
        regstate = node->constval->reg;
    else
        regstate = node->temp->reg;
    return regstate;
}
//�ж�һ���ڵ��Ƿ��ڼĴ�����
static bool inreg(Node * node)
{
    return get_reg(node) != 0;
}


//���reg���Լ���Ӧ�ı��������ȱ�
static void clear_reg(int reg)
{
    if(rvalue[reg].kind==RS_TMP)
        rvalue[reg].temp->reg = 0;
    else if(rvalue[reg].kind==RS_CONST)
        rvalue[reg].constval->reg = 0;

    rvalue[reg].kind = RS_NULL;
}
//���Ĵ����е����ݴ洢���ڴ棬���ɶ�Ӧָ�ע�ⲻ�����reg����������inmemory
static void store(int reg)
{
    if(rvalue[reg].kind==RS_TMP)
    {
        print("\tmov %s,",regname[reg]);
        print_tempaddr(codeout, rvalue[reg].temp);
        print("\n");
        rvalue[reg].temp->inmemory = 1;
    }
}
//����reg�Լ���Ӧ�ı��������ȱ�
static void set_reg(Node*node,int reg)
{
    if(node->kind==NK_NUM)
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
//�����ݴӴ洢���ж��뵽reg��
static void load(Node*node,int reg)
{
    if(node->kind==NK_NUM)
    {
        print("\tmov $%d,%s\n", node->constval->val, regname[reg]);
    }
    else 
    {
        print("\tmov ");
        print_tempaddr(codeout, node->temp);
        print(",%s\n", regname[reg]);
    }
}
//����һ���Ĵ���������Ĵ�������������Ҫ��ռĴ���
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

    Ӧ��Ҫ����һ��һ��ͳһ�ķ��� z=x op y  ��Ϊ���࣬��������ʱ������ͳ�����
    ��ΪҪ���������ļ���ʽ�ӣ�Ϊ�˷�ֹx��y֮���ͻ������Ӧ����һ�����������xy�ļĴ�������

    �����Ƿ���Ĵ��������ȿ���x�ڼĴ����У�������ʹ�øüĴ�����
        ���x�ǳ�������ֱ�ӷ���
        ���x����ʱ��������Ϊ���Ǳ�֤����ʱ����ֻʹ�úͶ���һ�Σ����Ա�Ȼ����ֱ�ӷ���
        ���x�Ǳ����������Ƿ���z��ͬ�������ͬ����ֱ�ӷ��䣬������Ҫ���ɱ���ָ��(Ҳ����Ҫ����)
    x������ڼĴ����У�����Ҫȥ�������Ĵ���

    Ϊx�������һ���Ĵ����Ĺ���
    ��ע��Z��Ȼ����һ��δ������ģ���Ϊ���ǳ����﷨����Z��Ȼ����Ҷ�ӽڵ㣬
    ��������ʱ�ڵ㣬���Ҹþ�Ϊ���壬���Ծͱ�Ȼ��ǰδ���������
    ����пռĴ�����ֱ�ӷ���
    ��������ˣ������ȷ���洢�����ļĴ��������Ҳ���Ҫ���ɱ��泣�������
    ���������ѡ��һ���滻��ȥ�������ɱ�����䡣

    Ȼ������y�ļĴ��������y�ڼĴ����������´󼪡�
    ������ڣ������һ���Ĵ�����ע�ⲻҪ��x���ͻ�����ɷ�ʽ��x��ͬ��


 * 
 */
//����2���������������
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
    default:
        break;
    }
}

//if value is in reg then just return 
//else chose a reg and load

/*
char * load(Node* node)
{
    if(node->kind==NK_NUM)
    {
        char * reg=get_reg();
        rvalue[regidx(reg)].kind = RS_VAL;
        rvalue[regidx(reg)].val = node->val;

        print("\tmovl %s,%d\n", reg, node->val);
        return reg;
    }
    else//when contain temp
    {
        if(node->temp->reg)
            return node->temp->reg;
        else{
            char *reg = get_reg();
            rvalue[regidx(reg)].kind = RS_TMP;
            rvalue[regidx(reg)].temp = node->temp;
            node->temp->reg = reg;
            return reg;
        }
            
    }
}
*/
void gen_code()
{

    print(".global main\n");
    print("main:\n");

    for (int i = 0; i < quadset->size; ++i)
    {
        Quad *quad = &quadset->list[i];
        switch (quad->op)
        {
        //���ڶ�Ԫ����z:=x op y ����һ�ζ���һ�����ã�����x�����reg���������Ϊz�ı���
        //������ڴ��У�����Ҫһ�μ��أ����ʱ�����һ���Ĵ������ɡ�
        case QK_ADD:
        case QK_SUB:
            gen_2operate_code(quad);
            break;
        default:
            break;
        }
    }

    Quad *quad = &quadset->list[quadset->size - 1];
    print("\tmov %s,%%eax\n", regname[get_reg(quad->result)]);
    print("\tret\n");
}
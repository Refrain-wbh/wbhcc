#include"wcc.h"

void error(char * fmt,...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(errout, fmt, ap);
    fprintf(errout, "\n");
}
void error_at(char * loc,char *fmt,...)
{
    va_list ap;
    va_start(ap, fmt);

    char *startp = loc, *endp = loc;
    while (startp > user_input && *(startp - 1) == '\n')
        startp--;
    while(*endp!='\0'&&*endp!='\n')
        endp++;

    fprintf(errout, "%.*s\n", (int)(endp - startp), startp);
    fprintf(errout,"%.*s^ ",(int)(loc-startp),"");
    vfprintf(errout, fmt, ap);
    fprintf(errout, "\n");
    exit(1);
}
bool at_eof()
{
    return curtoken->kind == TK_EOF;
}
int expect_num()
{
    if(curtoken->kind!=TK_NUM)
    {
        error_at(curtoken->str, "expected a number");
    }
    long val = curtoken->val;
    curtoken = curtoken->next;
    return val;
}
void expect(char *op)
{
    if(curtoken->kind!=TK_RESERVED || strlen(op)!=curtoken->strlen ||
            strncmp(curtoken->str,op,curtoken->strlen))
        error_at(curtoken->str,"expect \"%s\"",op);
    curtoken = curtoken->next;
}
bool consume(char *op)
{
    if(curtoken->kind!=TK_RESERVED|| strlen(op)!=curtoken->strlen ||   
                strncmp(curtoken->str,op,curtoken->strlen))
        return false;
    curtoken = curtoken->next;
    return true;
}
Token *consume_ident()
{
    if(curtoken->kind!=TK_IDENT)
        return NULL;
    Token *temp = curtoken;
    curtoken = curtoken->next;
    return temp;
}
//判断p是否是以q为开始
bool startwith(const char * p,const char * q)
{
    return strncmp(p, q, strlen(q)) == 0;
}

const char *start_with_reserved(const char *p)
{ 
    //关键字识别
    static const char *keys[] = {"return","if","else","while","for"};
    static const int keyscount = sizeof(keys)/ sizeof(keys[0]);
    for (int i = 0; i < keyscount; ++i)
    {
        int keylens = strlen(keys[i]);
        if (startwith(p, keys[i]) && !isalnum(p[keylens]))
        {
            return keys[i];
        }
    }
    
    //多字符分隔
    static const char *ops[] = {"==", ">=", "<=", "!="};
    static const int opslens = sizeof(ops) / sizeof(ops[0]);
    for (int i = 0; i < opslens;++i)
    {
        if(startwith(p,ops[i]))
            return ops[i];
    }
    return NULL;
}
Token* new_token(TokenKind kind,Token* cur,char * str , int strlen)
{
    Token *newtoken = calloc(1, sizeof(Token));
    cur->next = newtoken;
    newtoken->kind = kind;
    newtoken->str = str;
    newtoken->strlen = strlen;
    return newtoken;
}

Token *tokenize()
{
    Token head;
    Token *cur = &head;
    char *p = user_input;

    //接下来将input解析成为一系列的token
    while(*p)
    {
        if(isspace(*p))
        {
            p++;
            continue;
        }

        //保留字和多字符punct
        const char *reserved;
        if((reserved=start_with_reserved(p))!=NULL)
        {
            int lens = strlen(reserved);
            cur = new_token(TK_RESERVED, cur, p, lens);
            p += lens;
            continue;
        }
        
        //变量名识别
        if(isalpha(*p))
        {
            char *q=p;
            while(isalnum(*q))
                q++;
            cur = new_token(TK_IDENT, cur, p, q - p);
            p = q;
            continue;
        }
        if(isdigit(*p))
        {
            char *q = p;
            cur = new_token(TK_NUM, cur, p, 0);
            cur->val = strtol(p, &p, 10);
            cur->strlen = p - q;
            continue;
        }


        if(ispunct(*p)) //single char punct
        {
            cur = new_token(TK_RESERVED, cur, p, 1);
            p++;
            continue;
        }
        
        error_at(p, "invalid token");
        
    }
    cur = new_token(TK_EOF, cur, p, 0);

    
    /*printf("token list:\n");
    for (Token *i = head.next; i != NULL; i = i->next)
        printf("%d,%.*s,%d\n", i->kind, i->strlen, i->str,i->val);
    printf("token list end\n");*/
    return head.next;
}


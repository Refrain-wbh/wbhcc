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
//判断p是否是以q为开始
bool startwith(char * p,char * q)
{
    return strncmp(p, q, strlen(q)) == 0;
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
        }
        else if(startwith(p,"==") || //two char punct
                startwith(p,"!=") ||
                startwith(p,">=") ||
                startwith(p,"<="))
        {
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2;
        }
        else if(isdigit(*p))
        {
            char *q = p;
            cur = new_token(TK_NUM, cur, p, 0);
            cur->val = strtol(p, &p, 10);
            cur->strlen = p - q;
        }
        else if(ispunct(*p)) //single char punct
        {
            cur = new_token(TK_RESERVED, cur, p, 1);
            p++;
        }
        else 
        {
            error_at(p, "invalid token");
        }
    }
    cur = new_token(TK_EOF, cur, p, 0);

    /*
    printf("token list:\n");
    for (Token *i = head.next; i != NULL; i = i->next)
        printf("%d,%.*s,%d\n", i->kind, i->strlen, i->str,i->val);
    printf("token list end\n");*/
    return head.next;
}


#include"wcc.h"



static TempList* templist;
static ConstvalList *constvallist;

static int tempsize = 0;
Temp *new_temp()
{
    TempList *newnode = calloc(1, sizeof(TempList));
    newnode->next = templist;
    templist = newnode;
    newnode->temp.no = tempsize++;
    return &newnode->temp;
}

Constval *new_const()
{
    ConstvalList *newnode = calloc(1, sizeof(ConstvalList));
    newnode->next = constvallist;
    constvallist = newnode;
    return &newnode->constval;
}

void print_tempaddr(FILE*out,Temp*temp)
{
    fprintf(out, "T%d", temp->no);
}

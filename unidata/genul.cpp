/*

Generate UpperCase, and LowerCase unicode character ranges

genul <file> <name> <type>

*/

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ParseFields(char * s, char ** flds)
{
    int nflds = 0;

    for (;;)
    {
        char * f = strchr(s, ';');

        while (*s == ' ')
            s += 1;

        if (*s == 0)
            flds[nflds] = 0;
        else
            flds[nflds] = s;

        nflds += 1;
        if (f == 0)
            break;

        *f = 0;
        s = f + 1;
    }

    return(nflds);
}

typedef struct
{
    unsigned int Start;
    unsigned int End; // Inclusive
} CharRange;

CharRange Ranges[0x110000];
unsigned int NumRanges = 0;

void AddRange(char * fld)
{
    unsigned int strt = 0;
    unsigned int end = 0;

    while (*fld)
    {
        if (*fld >= '0' && *fld <= '9')
            strt = strt * 16 + *fld - '0';
        else if (*fld >= 'a' && *fld <= 'f')
            strt = strt * 16 + *fld - 'a' + 10;
        else if (*fld >= 'A' && *fld <= 'F')
            strt = strt * 16 + *fld - 'A' + 10;
        else
            break;

        fld += 1;
    }

    if (*fld == '.')
    {
        fld += 2;

        while (*fld)
        {
            if (*fld >= '0' && *fld <= '9')
                end = end * 16 + *fld - '0';
            else if (*fld >= 'a' && *fld <= 'f')
                end = end * 16 + *fld - 'a' + 10;
            else if (*fld >= 'A' && *fld <= 'F')
                end = end * 16 + *fld - 'A' + 10;
            else
                break;

            fld += 1;
        }
    }

    if (end == 0)
        end = strt;

    Ranges[NumRanges].Start = strt;
    Ranges[NumRanges].End = end;
    NumRanges += 1;
}

int MatchField(char * fld, char * s)
{
    char * r = strstr(fld, s);
    if (r == 0)
        return(0);

    if (r > fld && *(r - 1) != ' ')
        return(0);

    r += strlen(s);
    if (*r == ' ' || *r == 0)
        return(1);

    return(0);
}

void Usage()
{
    fprintf(stderr, "usage: genul <file> <name> <type>\n");
}

int main(int argc, char * argv[])
{
    char s[256];

    if (argc != 4)
    {
        Usage();
        return(1);
    }

    FILE * fp = fopen(argv[1], "rt");
    if (fp == 0)
    {
        fprintf(stderr, "error: genul: unable to open %s\n", argv[1]);
        return(1);
    }

    while (fgets(s, sizeof(s), fp))
    {
        char * flds[32];

        if (*s != '#' && *s != '\n')
        {
            int nflds = ParseFields(s, flds);

            if (nflds == 2 && MatchField(flds[1], argv[3]))
                AddRange(flds[0]);
        }
    }
    fclose(fp);

    printf("\nstatic FCharRange %s[%d] =\n{\n", argv[2], NumRanges);
    for (unsigned int ndx = 0; ndx < NumRanges; ndx++)
        printf("    {0x%x, 0x%x}, // %d\n", Ranges[ndx].Start, Ranges[ndx].End,
                Ranges[ndx].End - Ranges[ndx].Start + 1);
    printf("};\n");

    return(0);
}

/*

Generate Unicode Tables

genuni <file>

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

unsigned int ParseCodePoint(char * fld)
{
    char * s = fld;
    unsigned int n = 0;

    while (*fld)
    {
        if (*fld >= '0' && *fld <= '9')
            n = n * 16 + *fld - '0';
        else if (*fld >= 'a' && *fld <= 'f')
            n = n * 16 + *fld - 'a' + 10;
        else if (*fld >= 'A' && *fld <= 'F')
            n = n * 16 + *fld - 'A' + 10;
        else
        {
            printf("error: genuni: unable to parse field: %s\n", s);
            return(0);
        }

        fld += 1;
    }

    return(n);
}

void Usage()
{
    printf("usage: genuni <file> <index-field> <value-field> <first> <last>\n");
}

unsigned int Map[0x110000];

int main(int argc, char * argv[])
{
    char s[256];

    for (int idx = 0; idx < 0x110000; idx++)
        Map[idx] = idx;

    if (argc != 6)
    {
        Usage();
        return(1);
    }

    int idxfld = atoi(argv[2]);
    int valfld = atoi(argv[3]);

    int fst = ParseCodePoint(argv[4]);
    int lst = ParseCodePoint(argv[5]);

    FILE * fp = fopen(argv[1], "rt");
    if (fp == 0)
    {
        printf("error: genuni: unable to open %s\n", argv[1]);
        return(1);
    }

    while (fgets(s, sizeof(s), fp))
    {
        char * flds[32];

        if (*s != '#' && *s != '\n')
        {
            int nflds = ParseFields(s, flds);

            if (*flds[1] != 'C' && *flds[1] != 'S')
                continue;

            if (idxfld >= nflds)
            {
                printf("error: genuni: <index-field> too large: %d\n", idxfld);
                return(1);
            }

            if (valfld >= nflds)
            {
                printf("error: genuni: <value-field> too large: %d\n", valfld);
                return(1);
            }

            if (flds[idxfld] != 0 && flds[valfld] != 0)
            {
                unsigned int idx = ParseCodePoint(flds[idxfld]);
                unsigned int val = ParseCodePoint(flds[valfld]);
                Map[idx] = val;
            }
        }
    }

    while (fst <= lst)
    {
        printf("    0x%x, // 0x%x\n", Map[fst], fst);
        fst += 1;
    }

    fclose(fp);
    return(0);
}


/*

Generate CharAlphabectic Unicode Tables

genap <file> <type> <first> <last>

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

void ParseRange(char * fld, unsigned int *mp)
{
    char * s = fld;
    unsigned int n = 0;
    unsigned int m = 0;

    while (*fld)
    {
        if (*fld >= '0' && *fld <= '9')
            n = n * 16 + *fld - '0';
        else if (*fld >= 'a' && *fld <= 'f')
            n = n * 16 + *fld - 'a' + 10;
        else if (*fld >= 'A' && *fld <= 'F')
            n = n * 16 + *fld - 'A' + 10;
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
                m = m * 16 + *fld - '0';
            else if (*fld >= 'a' && *fld <= 'f')
                m = m * 16 + *fld - 'a' + 10;
            else if (*fld >= 'A' && *fld <= 'F')
                m = m * 16 + *fld - 'A' + 10;
            else
                break;

            fld += 1;
        }
    }

    if (m == 0)
        mp[n] = 1;
    else
    {
        while (n <= m)
        {
            mp[n] = 1;
            n += 1;
        }
    }
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
            fprintf(stderr, "error: genap: unable to parse field: %s\n", s);
            return(0);
        }

        fld += 1;
    }

    return(n);
}

void Usage()
{
    fprintf(stderr, "usage: genap <file> <type> <first> <last>\n");
}

unsigned int Map[0x110000];

int main(int argc, char * argv[])
{
    char s[256];

    for (int idx = 0; idx < 0x110000; idx++)
        Map[idx] = 0;

    if (argc != 5)
    {
        Usage();
        return(1);
    }

    int fst = ParseCodePoint(argv[3]);
    int lst = ParseCodePoint(argv[4]);

    if (fst % 32 != 0)
    {
        fprintf(stderr, "error: genap: first must be divible by 32\n");
        return(1);
    }

    FILE * fp = fopen(argv[1], "rt");
    if (fp == 0)
    {
        fprintf(stderr, "error: genap: unable to open %s\n", argv[1]);
        return(1);
    }

    while (fgets(s, sizeof(s), fp))
    {
        char * flds[32];

        if (*s != '#' && *s != '\n')
        {
            int nflds = ParseFields(s, flds);

            if (nflds == 2 && strstr(flds[1], argv[2]))
                ParseRange(flds[0], Map);
        }
    }

    printf("static const unsigned int Alphabetic0x%04xTo0x%04x[] =\n{\n", fst,
            (lst / 32) * 32 + 31);

    while (fst <= lst)
    {
        unsigned int msk = 0;

        for (int idx = 0; idx < 32; idx++)
            if (Map[fst + idx])
                msk |= 1 << idx;

        if (msk == 0)
            printf("    0x0, // 0x%04x\n", fst);
        else
            printf("    0x%08x, // 0x%04x\n", msk, fst);
        fst += 32;
    }

    printf("    0x0\n};\n");

    fclose(fp);
    return(0);
}

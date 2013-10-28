/*

Generate Upcase, Downcase and Foldcase Unicode Tables

genudf <file> Upcase|Downcase|Foldcase|Fullfold|Fullup|Fulldown <field> <max-gap>

*/

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FULL_CHARS 3

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
            fprintf(stderr, "error: genudf: unable to parse field: %s\n", s);
            return(0);
        }

        fld += 1;
    }

    return(n);
}

unsigned int ParseSeveralPoints(char * fld, unsigned int chars[MAX_FULL_CHARS])
{
    unsigned int cnt = 0;
    char * s;

    for (;;)
    {
        while (*fld == ' ')
            fld += 1;

        s = strchr(fld, ' ');
        if (s)
            *s = 0;

        chars[cnt] = ParseCodePoint(fld);
        cnt += 1;

        if (s == 0)
            break;

        fld = s + 1;
    }

    return(cnt);
}

void Usage()
{
    fprintf(stderr,
            "usage: genudf <file> Upcase|Downcase|Foldcase|Fullfold|Fullup|Fulldown  <field> <max-gap>\n");
}

typedef struct
{
    unsigned int Count;
    unsigned int Chars[MAX_FULL_CHARS];
} FullChar;

unsigned int Map[0x110000];
FullChar FullMap[0x110000];

int main(int argc, char * argv[])
{
    char s[256];

    for (int idx = 0; idx < 0x110000; idx++)
    {
        Map[idx] = idx;

        FullMap[idx].Count = 1;
        FullMap[idx].Chars[0] = idx;
        FullMap[idx].Chars[1] = 0;
        FullMap[idx].Chars[2] = 0;
    }

    if (argc != 5)
    {
        Usage();
        return(1);
    }

    if (strcmp(argv[2], "Upcase") && strcmp(argv[2], "Downcase") && strcmp(argv[2], "Foldcase")
            && strcmp(argv[2], "Fullfold") && strcmp(argv[2], "Fullup")
            && strcmp(argv[2], "Fulldown"))
    {
        fprintf(stderr,
                "error: genudf: expected 'Upcase', 'Downcase', 'Foldcase', 'Fullfold', 'Fullup', or 'Fulldown'\n");
        return(1);
    }

    int fcf = (strcmp(argv[2], "Foldcase") == 0 || strcmp(argv[2], "Fullfold") == 0);
    int ff = (strcmp(argv[2], "Fullfold") == 0 || strcmp(argv[2], "Fullup") == 0
            || strcmp(argv[2], "Fulldown") == 0);
    int fdx = atoi(argv[3]);

    unsigned int maxgap = ParseCodePoint(argv[4]);

    FILE * fp = fopen(argv[1], "rt");
    if (fp == 0)
    {
        fprintf(stderr, "error: genudf: unable to open %s\n", argv[1]);
        return(1);
    }

    while (fgets(s, sizeof(s), fp))
    {
        char * flds[32];

        if (*s != '#' && *s != '\n')
        {
            int nflds = ParseFields(s, flds);

            if (fcf)
            {
                if (ff)
                {
                    if (*flds[1] != 'F')
                        continue;
                }
                else if (*flds[1] != 'C' && *flds[1] != 'S')
                    continue;
            }

            if (nflds > 5 && (strcmp(argv[2], "Fullup") == 0 || strcmp(argv[2], "Fulldown") == 0))
                continue;

            if (fdx >= nflds)
            {
                fprintf(stderr, "error: genudf: <field> too large: %d\n", fdx);
                return(1);
            }

            if (*flds[fdx])
            {
                unsigned int idx = ParseCodePoint(flds[0]);

                if (ff)
                {
                    FullMap[idx].Count = ParseSeveralPoints(flds[fdx], FullMap[idx].Chars);
                    Map[idx] = FullMap[idx].Chars[0];

                    if (FullMap[idx].Count == 0)
                    {
                        fprintf(stderr, "error: genudf: unexpected full mapping: 0x%04x\n", idx);
                        return(1);
                    }
                }
                else
                {
                    unsigned int val = ParseCodePoint(flds[fdx]);
                    Map[idx] = val;
                }
            }
        }
    }

    unsigned int Start[128];
    unsigned int End[128];
    unsigned int cnt = 0;

    unsigned int tot = 0;
    unsigned int idx = 0;
    while (idx < 0x110000)
    {
        while (Map[idx] == idx)
        {
            idx += 1;
            if (idx == 0x110000)
                break;
        }

        unsigned int strt = idx;
        unsigned int end;
        unsigned int gap = 0;
        for (; idx < 0x110000; idx++)
        {
            if (Map[idx] == idx)
            {
                gap += 1;
                if (gap > maxgap)
                    break;
            }
            else
            {
                end = idx;
                gap = 0;
            }
        }

        if (idx < 0x110000)
        {
            Start[cnt] = strt;
            End[cnt] = end;
            cnt += 1;

            fprintf(stderr, "0x%04x --> 0x%04x [%d]\n", strt, end, end - strt);
            tot += (end - strt);
        }
    }

//    printf("%d\n", tot);

    if (ff)
    {
        printf("#ifndef __FFULLCASE__\n");
        printf("#define __FFULLCASE__\n");
        printf("typedef struct {unsigned int Count; FCh Chars[3];} FFullCase;\n");
        printf("#endif\n\n");

        unsigned int fdx = 0;

        printf("static const unsigned int %sSet[] =\n{\n", argv[2]);

        while (fdx <= 0x1FFF)
        {
            unsigned int msk = 0;

            for (int idx = 0; idx < 32; idx++)
                if (FullMap[fdx + idx].Count > 1)
                    msk |= (1 << idx);

            if (msk == 0)
                printf("    0x0, // 0x%04x\n", fdx);
            else
                printf("    0x%08x, // 0x%04x\n", msk, fdx);
            fdx += 32;
        }

        printf("    0x0\n};\n\n");

        for (unsigned int cdx = 0; cdx < cnt; cdx++)
        {
            printf("static FFullCase %s0x%04xTo0x%04x[] =\n{\n", argv[2], Start[cdx], End[cdx]);

            for (idx = Start[cdx]; idx < End[cdx]; idx++)
                printf("    {%d, {0x%04x, 0x%04x, 0x%04x}}, // 0x%04x\n", FullMap[idx].Count,
                        FullMap[idx].Chars[0], FullMap[idx].Chars[1], FullMap[idx].Chars[2], idx);
            printf("    {%d, {0x%04x, 0x%04x, 0x%04x}} // 0x%04x\n};\n\n", FullMap[idx].Count,
                    FullMap[idx].Chars[0], FullMap[idx].Chars[1], FullMap[idx].Chars[2], idx);
        }
    }
    else
    {
        for (unsigned int cdx = 0; cdx < cnt; cdx++)
        {
            printf("static const FCh %s0x%04x[] =\n{\n", argv[2], Start[cdx]);

            for (idx = Start[cdx]; idx < End[cdx]; idx++)
                printf("    0x%04x, // 0x%04x\n", Map[idx], idx);
            printf("    0x%04x  // 0x%04x\n};\n\n", Map[idx], idx);
        }

        printf("FCh Char%s(FCh ch)\n{\n", argv[2]);
        for (unsigned int cdx = 0; cdx < cnt; cdx++)
        {
            printf("    if (ch <= 0x%04x)\n    {\n", End[cdx]);
            printf("        if (ch >= 0x%04x)\n",  Start[cdx]);
            printf("            return(%s0x%04x[ch - 0x%04x]);\n", argv[2], Start[cdx], Start[cdx]);
            printf("        return(ch);\n    }\n");
        }
        printf("    return(ch);\n}\n\n");
    }

    fclose(fp);
    return(0);
}

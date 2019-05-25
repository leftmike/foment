/*

Generate character ranges for a char-set

gencrng <file> <name> <letter-category> | +<character-code> ...

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
            fprintf(stderr, "error: gencrng: unable to parse field: %s\n", s);
            return(0);
        }

        fld += 1;
    }

    return(n);
}

void Usage()
{
    fprintf(stderr, "usage: gencrng <file> <name> <letter-category> | <character-code> ...\n");
}

typedef struct
{
    unsigned int Start;
    unsigned int End; // Inclusive
} CharRange;

CharRange Ranges[0x110000];
unsigned int NumRanges = 0;

void AddCh(unsigned int ch)
{
    if (NumRanges == 0 || Ranges[NumRanges - 1].End + 1 < ch)
    {
        Ranges[NumRanges].Start = ch;
        Ranges[NumRanges].End = ch;
        NumRanges += 1;
    }
    else
        Ranges[NumRanges-1].End += 1;
}

int main(int argc, char * argv[])
{
    char s[256];

    if (argc < 4)
    {
        Usage();
        return(1);
    }

    FILE * fp = fopen(argv[1], "rt");
    if (fp == 0)
    {
        fprintf(stderr, "error: gencrng: unable to open %s\n", argv[1]);
        return(1);
    }

    while (fgets(s, sizeof(s), fp))
    {
        char * flds[32];

        if (*s != '#' && *s != '\n')
        {
            int nflds = ParseFields(s, flds);
            if (nflds < 3)
            {
                fprintf(stderr, "error: gencrng: not enough fields %s\n", flds[0]);
                return(1);
            }

            for (int adx = 3; adx < argc; adx++)
                if (strcmp(argv[adx], flds[2]) == 0 ||
                    (argv[adx][0] == '+' && strcmp(argv[adx] + 1, flds[0]) == 0))
                {
                    AddCh(ParseCodePoint(flds[0]));
                    break;
                }
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

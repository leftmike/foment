/*

Convert a text file into a multiple line string constant.

txt2cpp <input> <output> <name>

*/

#ifdef FOMENT_WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif // FOMENT_WIN32

#include <stdio.h>

int main(int argc, char * argv[])
{
    char buf[256];

    if (argc != 4)
    {
        printf("usage: txt2cpp <input> <output> <name>\n");
        return(1);
    }

    FILE * inf = fopen(argv[1], "r");
    if (inf == 0)
    {
        printf("error: unable to open %s for reading\n", argv[1]);
        return(1);
    }

    FILE * outf = fopen(argv[2], "w");
    if (outf == 0)
    {
        printf("error: unable to open %s for writing\n", argv[2]);
        return(1);
    }

    fprintf(outf, "char %s[] =", argv[3]);
    for (;;)
    {
        if (fgets(buf, sizeof(buf), inf) == 0)
            break;

        char * s = buf;
        int cnt = 0;
        while (*s)
        {
            if (*s != ' ')
                break;

            s += 1;
            cnt += 1;
        }

        if (*s != 0 && *s != '\n')
        {
            fputc('\n', outf);

            while (cnt > 0)
            {
                fputc(' ', outf);
                cnt -= 1;
            }

            fputc('"', outf);

            while (*s)
            {
                if (*s == '\n')
                {
                    fputc('\\', outf);
                    fputc('n', outf);
                    break;
                }

                if (*s == '"')
                    fputc('\\', outf);
                fputc(*s, outf);

                s += 1;
            }

            fputc('"', outf);
        }
    }

    fputc(';', outf);
    fputc('\n', outf);

    if (fclose(inf) != 0)
    {
        printf("error: unable to close %s\n", argv[1]);
        return(1);
    }

    if (fclose(outf) != 0)
    {
        printf("error: unable to close %s\n", argv[2]);
        return(1);
    }

    return(0);
}

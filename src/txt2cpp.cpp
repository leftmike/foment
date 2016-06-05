/*

Convert a text file into a multiple line string constant.

txt2cpp <input> <output> <name>

*/

#ifdef FOMENT_WINDOWS
#define _CRT_SECURE_NO_WARNINGS
#endif // FOMENT_WINDOWS

#include <stdio.h>
#include <string.h>

void print_name(FILE * outf, char * nam)
{
    int sf = 1;

    while (*nam)
    {
        if (*nam >= 'a' && *nam <= 'z')
        {
            if (sf)
                fputc(*nam - 'a' + 'A', outf);
            else
                fputc(*nam, outf);
            sf = 0;
        }
        else if (*nam == ' ' || *nam == '(' || *nam == '-')
            sf = 1;
        else if (*nam == ')')
            break;
        else
        {
            fputc(*nam, outf);
            sf = 0;
        }

        nam += 1;
    }
}

int main(int argc, char * argv[])
{
    char buf[256];
    char * libs[256];
    int num_libs = 0;
    int idx;

    if (argc != 3)
    {
        printf("usage: txt2cpp <input> <output>\n");
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

    for (;;)
    {
        if (fgets(buf, sizeof(buf), inf) == 0)
            break;

        if (strncmp(buf, "(define-library", 15) == 0)
        {
            idx = 15;
            while (buf[idx] != 0 && buf[idx] != '(')
                idx += 1;

            if (buf[idx] != 0)
            {
                char * nam = strdup(buf + idx);
                for (idx = 0; nam[idx] != 0; idx++)
                    if (nam[idx] == ')')
                    {
                        nam[idx + 1] = 0;
                        break;
                    }

                if (num_libs > 0)
                {
                    fputc(';', outf);
                    fputc('\n', outf);
                }

                fprintf(outf, "char ");
                print_name(outf, nam);
                fprintf(outf, "[] =");
                libs[num_libs] = nam;
                num_libs += 1;
            }
        }

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

                if (*s == '"' || *s == '\\')
                    fputc('\\', outf);
                fputc(*s, outf);

                s += 1;
            }

            fputc('"', outf);
        }
    }

    fputc(';', outf);
    fputc('\n', outf);

    fprintf(outf, "char * BuiltinLibraries[] = {\n");
    for (idx = 1; idx < num_libs; idx++)
    {
        fprintf(outf, "    ");
        print_name(outf, libs[idx]);
        fprintf(outf, ",\n");
    };
    fprintf(outf, "    0\n};\n");

    fprintf(outf, "char BuiltinLibraryNames[] =\n\"#(\"\n");
    for (idx = 1; idx < num_libs; idx++)
        fprintf(outf, "\"    %s\"\n", libs[idx]);
    fprintf(outf, "\")\";\n");

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

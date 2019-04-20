/*

Convert a scheme library into a multiple line string constant.

*/

#ifdef FOMENT_WINDOWS
#define _CRT_SECURE_NO_WARNINGS
#define strdup(s) _strdup(s)
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

char * parse_name(char * s)
{
    int idx = 0;

    while (s[idx] != 0 && s[idx] != '(')
        idx += 1;

    if (s[idx] != 0)
    {
        char * nam = strdup(s + idx);
        for (idx = 0; nam[idx] != 0; idx++)
            if (nam[idx] == ')')
            {
                nam[idx + 1] = 0;
                break;
            }

        return(nam);
    }

    return(0);
}

int main(int argc, char * argv[])
{
    char buf[256];
    char * libs[256];
    char * names[256];
    int num_libs = 0;
    int idx;

    if (argc < 3)
    {
        printf("usage: txt2cpp <output> <input>...\n");
        return(1);
    }

    FILE * outf = fopen(argv[1], "w");
    if (outf == 0)
    {
        printf("error: unable to open %s for writing\n", argv[1]);
        return(1);
    }

    for (int adx = 2; adx < argc; adx += 1)
    {
        FILE * inf = fopen(argv[adx], "r");
        if (inf == 0)
        {
            printf("error: unable to open %s for reading\n", argv[adx]);
            return(1);
        }

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

            if (strncmp(s, "(define-library", 15) == 0)
            {
                char * nam = parse_name(s + 15);
                if (nam != 0)
                {
                    if (num_libs > 0)
                    {
                        fputc(';', outf);
                        fputc('\n', outf);
                    }

                    fprintf(outf, "char ");
                    print_name(outf, nam);
                    fprintf(outf, "[] =");
                    libs[num_libs] = nam;
                    names[num_libs] = nam;
                    num_libs += 1;
                }
            }
            else if (strncmp(s, "(aka", 4) == 0 && num_libs > 0)
            {
                char * nam = parse_name(s + 4);
                if (nam != 0)
                {
                    libs[num_libs] = libs[num_libs - 1];
                    names[num_libs] = nam;
                    num_libs += 1;
                }
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

        if (fclose(inf) != 0)
        {
            printf("error: unable to close %s\n", argv[adx]);
            return(1);
        }
    }

    fprintf(outf, "char * FomentLibraries[] = {\n");
    for (idx = 1; idx < num_libs; idx++)
    {
        fprintf(outf, "    ");
        print_name(outf, libs[idx]);
        fprintf(outf, ",\n");
    };
    fprintf(outf, "    0\n};\n");

    fprintf(outf, "char FomentLibraryNames[] =\n\"#(\"\n");
    for (idx = 1; idx < num_libs; idx++)
        fprintf(outf, "\"    %s\"\n", names[idx]);
    fprintf(outf, "\")\";\n");

    if (fclose(outf) != 0)
    {
        printf("error: unable to close %s\n", argv[1]);
        return(1);
    }

    return(0);
}

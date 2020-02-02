/*

Write lines to standard output or error; each line is an argument.

*/

#include <string.h>
#include <stdio.h>

int main(int argc, char * argv[])
{
    FILE * fp;

    if (argc < 2)
        return(1);
    if (strcmp(argv[1], "--stdout") == 0)
        fp = stdout;
    else if (strcmp(argv[1], "--stderr") == 0)
        fp = stderr;
    else
        return(2);

    for (int adx = 2; adx < argc; adx += 1)
    {
        fputs(argv[adx], fp);
        fputc('\n', fp);
    }

    return(0);
}

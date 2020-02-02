/*

Read lines from standard input; each line should match one argument.

*/

#include <string.h>
#include <stdio.h>

int main(int argc, char * argv[])
{
    char s[1024];
    int adx = 1;

    for (;;)
    {
        if (fgets(s, sizeof(s), stdin) == 0)
            break;
        if (adx == argc)
            return(1);
        size_t sl = strlen(s);
        if (sl > 0)
            s[sl - 1] = 0;
        if (strcmp(s, argv[adx]) != 0)
            return(2);
        adx += 1;
    }

    return(0);
}

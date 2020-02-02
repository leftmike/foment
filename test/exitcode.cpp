/*

Exit with the code specified as an argument.

*/

#include <stdlib.h>

int main(int argc, char * argv[])
{
    if (argc != 2)
        return(1);
    return(atoi(argv[1]));
}

/*

Generate buildprops.cpp based on buildprops.out

*/

#include <stdio.h>
#include <string.h>

const char * props[] =
{
#ifdef FOMENT_WINDOWS
    "build.branch",
    "build.commit",
    0,
    "build.platform",
    "c.version"
#else // FOMENT_WINDOWS

#endif // FOMENT_WINDOWS
};

int main(int argc, char * argv[])
{
    char buf[256];

    printf("// Do not modify; generated file\n\n");
    // Size must match BuildProperties in src/main.cpp
    printf("const char * BuildProperties[4] =\n{\n");
    for (int idx = 0; idx < sizeof(props) / sizeof(char *); idx += 1)
    {
        fgets(buf, sizeof(buf), stdin);
        if (props[idx] == 0)
            continue;

        if (strlen(buf) == 0)
            return(1);

        buf[strlen(buf)-1] = 0;
        if (idx > 0)
            printf(",\n");
        printf("    \"(%s \\\"%s\\\")\"", props[idx], buf);
    }
    printf("\n};\n");

    return(0);
}


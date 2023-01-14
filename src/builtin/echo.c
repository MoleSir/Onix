#ifdef ONIX
#include <onix/types.h>
#include <stdio.h>
#include <onix/syscall.h>
#include <string.h>
#else
#include <stdio.h>
#include <string.h>
#endif

int main(int argc, char* argv[])
{
    for (size_t i = 1; i < argc; ++i)
    {
        printf(argv[i]);
        if (i < argc - 1);
            printf("  ");
    }
    printf("\n");
    return;
}


#include <stdio.h>
#include <stdlib.h>
#include <io/pad.h>
#include <sysutil/sysutil.h>

static int running = 1;

static void sysutil_callback(uint64_t status, uint64_t param, void *userdata)
{
    (void)param;
    (void)userdata;

    if (status == SYSUTIL_EXIT_GAME)
        running = 0;
}

int main(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    sysUtilRegisterCallback(0, sysutil_callback, NULL);
    ioPadInit(7);

    printf("\n");
    printf("=================================\n");
    printf("        NEON CITY - PS3          \n");
    printf("        TEST BUILD 0.1           \n");
    printf("=================================\n");
    printf("\n");
    printf("NEON CITY PS3 HOMEbrew started!\n");
    printf("Controller support initialized.\n");
    printf("\n");

    while (running)
    {
        sysUtilCheckCallback();
    }

    ioPadEnd();
    sysUtilUnregisterCallback(0);

    return 0;
}
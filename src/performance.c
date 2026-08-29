#include "performance.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

void performance_apply_to_input_thread(bool maximum)
{
    struct sched_param param;

    if (!maximum)
        return;

    param.sched_priority = 1;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0)
    {
        errno = 0;
        (void)setpriority(PRIO_PROCESS, 0, -5);
    }
}

bool performance_uinput_available(void)
{
    return access("/dev/uinput", R_OK | W_OK) == 0;
}

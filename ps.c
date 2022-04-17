#include "types.h"
#include "stat.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"
#include "user.h"
int main(int argc, char *argv[])
{
    char *states[6] = {"UNUSED",
                       "EMBRYO",
                       "SLEEPING",
                       "RUNNABLE",
                       "RUNNING",
                       "ZOMBIE"};
    struct proc *p = (struct proc *)malloc(sizeof(struct proc) * NPROC);
    getptable(p);
    printf(1, "PID \t CWD \t stat\n");
    printf(1, "--------------------------------------------------------\n");
    for (int i = 0; i < NPROC; ++i)
    {
        if (argc == 1)
        {
            if (p[i].state == RUNNING)
            {
                printf(1, "%d \t %s \t %s\n", p[i].pid, p[i].name, states[p[i].state]);
            }
        }
        else if (argc > 1)
        {
            if (strcmp(argv[1], "-a") == 0)
            {
                printf(1, "%d \t %s \t %s\n", p[i].pid, p[i].name, states[p[i].state]);
            }
            else
            {
                printf(2, "Wrong option.");
                exit();
            }
        }
    }
    exit();
}
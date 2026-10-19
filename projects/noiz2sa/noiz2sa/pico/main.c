#include "noiz2sa_port_target.h"

int main(void)
{
    /* Target bootstrap is intentionally separate from the game archive. */
    return noiz2sa_port_target_run();
}

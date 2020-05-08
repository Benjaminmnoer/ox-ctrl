#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/queue.h>
#include <libox.h>
#include <ox-mq.h>
#include <ox-app.h>

static int delta_write(struct nvm_io_cmd *cmd)
{
    return 0;
}

static int delta_init()
{
    return 0;
}

static void delta_exit()
{
    return;
}

static struct app_delta oxblk_delta = {
    .mod_id = OXBLK_DELTA,
    .name = "OX_BLOCK_DELTA",
    .init_fn = delta_init,
    .exit_fn = delta_exit,
    .write_fn = delta_write
};

void oxb_delta_register (void) {
    app_mod_register (APPMOD_DELTA, OXBLK_DELTA, &oxblk_delta);
}
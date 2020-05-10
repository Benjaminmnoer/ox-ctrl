#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/queue.h>
#include <libox.h>
#include <ox-mq.h>
#include <ox-app.h>

#define DELTA_CMDS          64
#define DELTA_RETRY         40000
#define DELTA_RETRY_DELAY   100

static struct ox_mq        *delta_mq;

struct delta_cmd {
    struct nvm_io_cmd           *nvme_cmd;    

    pthread_spinlock_t           spin;
    struct ox_mq_entry         *mentry;
    STAILQ_ENTRY(delta_cmd)    fentry;
    TAILQ_ENTRY(delta_cmd)     uentry;
};

STAILQ_HEAD(fcmd_q, delta_cmd) fdeltahead = STAILQ_HEAD_INITIALIZER(fdeltahead);
TAILQ_HEAD(ucmd_q, delta_cmd) udeltahead = TAILQ_HEAD_INITIALIZER(udeltahead);
static pthread_spinlock_t cmd_spin;

static void delta_mq_to (void **opaque, int c)
{
    printf("Have i done something correct? Delta MQ to function called.\n");
    struct delta_cmd *cmd;
    cmd = opaque[0];
    cmd->nvme_cmd->status.status = NVM_IO_FAIL;
    cmd->nvme_cmd->status.nvme_status = NVME_MEDIA_TIMEOUT;
}

static void delta_sq (struct ox_mq_entry *req)
{
    printf("Have i done something correct? Delta MQ sq function called.\n");
    struct delta_cmd *cmd = (struct delta_cmd *)req->opaque;
    cmd->mentry = req;

    pthread_mutex_lock (&cmd->nvme_cmd->mutex);
    cmd->nvme_cmd->status.status = NVM_IO_FAIL;
    cmd->nvme_cmd->status.nvme_status = NVME_INTERNAL_DEV_ERROR;
    pthread_mutex_unlock (&cmd->nvme_cmd->mutex);
    ox_mq_complete_req(delta_mq, cmd->mentry);
}

static void delta_sq_cb (void *opaque)
{
    printf("Have i done something correct? Delta MQ sq cb function called.\n");
    struct delta_cmd *cmd = (struct delta_cmd *) opaque;
    struct nvm_io_cmd *nvme_cmd = cmd->nvme_cmd;
    pthread_mutex_lock (&nvme_cmd->mutex);

    ox_ftl_callback (nvme_cmd);

    pthread_mutex_unlock (&nvme_cmd->mutex);

    pthread_spin_lock (&cmd_spin);
    TAILQ_REMOVE(&udeltahead, cmd, uentry);
    STAILQ_INSERT_TAIL(&fdeltahead, cmd, fentry);
    pthread_spin_unlock (&cmd_spin);
}

static void delta_stats_fill_row (struct oxmq_output_row *row, void *opaque)
{
    row->lba = 0;
    row->ch = 0;
    row->lun = 0;
    row->blk = 0;
    row->pg = 0;
    row->pl = 0;
    row->sec = 0;
    row->type = 'D';
    row->failed = 0;
    row->datacmp = 0;
    row->size = 0;
}

static struct ox_mq_config delta_mq_config = {
    .name       = "DELTA",
    .n_queues   = 1,
    .q_size     = 65536,
    .sq_fn      = delta_sq,
    .cq_fn      = delta_sq_cb,
    .to_fn      = delta_mq_to,
    .output_fn  = delta_stats_fill_row,
    .to_usec    = 4000000,
    .flags      = OX_MQ_CPU_AFFINITY
};

static void delta_callback(struct nvm_io_cmd *cmd)
{
    struct delta_cmd *delta_cmd;
    delta_cmd = (struct delta_cmd *)cmd;
    ox_mq_complete_req(delta_mq, delta_cmd->mentry);
}

static int delta_submit(struct nvm_io_cmd *cmd)
{
    printf("Delta component received submit request.\n");
    uint32_t retry = DELTA_RETRY;
    struct delta_cmd *delta_cmd;

RETRY:
    pthread_spin_lock (&cmd_spin);
    if (STAILQ_EMPTY(&fdeltahead)) {
        pthread_spin_unlock (&cmd_spin);
        retry--;
        usleep (DELTA_RETRY_DELAY);
        if (retry) {
            goto RETRY;
        } else
            return -1; // TODO Requeue command if fail.
    }

    delta_cmd = STAILQ_FIRST(&fdeltahead);
    if (!delta_cmd) {
        pthread_spin_unlock (&cmd_spin);
        retry--;
        usleep (DELTA_RETRY_DELAY);
        if (retry) {
            goto RETRY;
        } else
            return -1; // TODO Requeue command if fail.
    }

    STAILQ_REMOVE_HEAD (&fdeltahead, fentry);
    TAILQ_INSERT_TAIL(&udeltahead, delta_cmd, uentry);
    pthread_spin_unlock (&cmd_spin);

    delta_cmd->nvme_cmd = cmd;

    if (ox_mq_submit_req(delta_mq, 0, delta_cmd)){
        return -1;
    }

    return 0;
}

static void delta_exit()
{
    return;
}

static int delta_init()
{
    struct delta_cmd *cmd;
    uint8_t cmd_i;

    if (!ox_mem_create_type ("OXBLK_DELTA", OX_MEM_OXBLK_DELTA))
        return -1;


    STAILQ_INIT(&fdeltahead);
    TAILQ_INIT(&udeltahead);

    if (pthread_spin_init(&cmd_spin, 0)){
        return -1;
    }
    
    for (cmd_i = 0; cmd_i < DELTA_CMDS; cmd_i++){
        cmd = ox_calloc (sizeof (struct delta_cmd), 1, OX_MEM_OXBLK_DELTA);
        if (!cmd) goto FREE_CMD;    

        if (pthread_spin_init(&cmd->spin, 0)) {
            ox_free (cmd, OX_MEM_OXBLK_LBA);
            goto FREE_CMD;
        }

        STAILQ_INSERT_TAIL(&fdeltahead, cmd, fentry);
    }

    /* Set thread affinity, if enabled */
    delta_mq_config.sq_affinity[0] = 0;
    delta_mq_config.cq_affinity[0] = 0;

#if OX_TH_AFFINITY
    delta_mq_config.sq_affinity[0] |= ((uint64_t) 1 << 0);
	delta_mq_config.sq_affinity[0] |= ((uint64_t) 1 << 6);

    delta_mq_config.cq_affinity[0] |= ((uint64_t) 1 << 0);
#endif /* OX_TH_AFFINITY */

    delta_mq = ox_mq_init(&delta_mq_config);
    if (!delta_mq){
      delta_exit();
      return -1;  
    } 

    return 0;

FREE_CMD:
    ox_free (cmd, OX_MEM_OXBLK_DELTA);
    pthread_spin_destroy (&cmd->spin);
    return -1;
}

static struct app_delta oxblk_delta = {
    .mod_id = OXBLK_DELTA,
    .name = "OX_BLOCK_DELTA",
    .init_fn = delta_init,
    .exit_fn = delta_exit,
    .submit_fn = delta_submit
};

void oxb_delta_register (void) {
    app_mod_register (APPMOD_DELTA, OXBLK_DELTA, &oxblk_delta);
}
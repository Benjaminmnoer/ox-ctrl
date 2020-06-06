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

static uint32_t             npages;
extern uint16_t             app_nch;
static struct app_channel **ch;

struct delta_page {
    struct nvm_io_cmd          *nvme_cmd;
    void                       *data; // TODO: Double check.

    struct ox_mq_entry         *mentry;
};

struct delta_md_entry {
    uint16_t                    block_offset;
    uint16_t                    page_offset;
    uint16_t                    size;
};

struct delta_md {
    struct delta_md_entry      *entries;
    uint16_t                    nentries;
    uint64_t                    basepage;
    uint64_t                    previous_chain;
};

// This is the write buffer struct.
struct delta_cmd {
    struct nvm_io_cmd          *nvme_cmd;
    struct delta_page         **pages;
    uint16_t                    npages;
    struct delta_md           **md;
    uint16_t                    nmd;

    pthread_spinlock_t          spin;
};

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
    uint32_t retry = DELTA_RETRY;
    struct delta_cmd *cmd = (struct delta_cmd *)req->opaque;
    struct delta_cmd *cmdz;
    struct app_map_entry *map_entry;
    struct app_prov_ppas *prov;


    
    map_entry = oxapp()->gl_map->read_fn(cmd->nvme_cmd->slba);
    // TODO: Complete request. Use mentry from somewhere, possibly delta_page.
}

static void delta_sq_cb (void *opaque)
{
    printf("Have i done something correct? Delta MQ sq cb function called.\n");
    struct delta_cmd *cmd = (struct delta_cmd *) opaque;
    struct nvm_io_cmd *nvme_cmd = cmd->nvme_cmd;

    ox_ftl_callback (nvme_cmd);
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
    .q_size     = 64,
    .sq_fn      = delta_sq,
    .cq_fn      = delta_sq_cb,
    .to_fn      = delta_mq_to,
    .output_fn  = delta_stats_fill_row,
    .to_usec    = 4000000,
    .flags      = OX_MQ_CPU_AFFINITY
};

static void delta_callback(struct nvm_io_cmd *cmd)
{
    // TODO: What does this function do?
    struct delta_cmd *delta_cmd;
    delta_cmd = (struct delta_cmd *)cmd;
}

static int delta_submit(struct nvm_io_cmd *cmd)
{
    printf("Delta component received submit request.\n");
    uint32_t retry = DELTA_RETRY;
    struct delta_cmd *delta_cmd;

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
    // TODO: What is necessary to initialize?
    struct delta_cmd *cmd;
    struct delta_page *page;
    uint32_t cmd_i, page_i, ret;

    if (!ox_mem_create_type ("OXBLK_DELTA", OX_MEM_OXBLK_DELTA))
        return -1;

    ch = ox_malloc (sizeof(struct app_channel *) * app_nch, OX_MEM_OXBLK_LBA);
    if (!ch)
        return -1;

    ret = oxapp()->channels.get_list_fn (ch, app_nch);
    if (ret != app_nch)
        goto FREE_CH;

    npages = ch[0]->ch->geometry->pg_per_blk;

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

    log_info("    [appnvm: Delta started.]\n");

    return 0;

FREE_CH:
    ox_free (ch, OX_MEM_OXBLK_LBA);
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
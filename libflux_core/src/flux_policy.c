#include "uft/uft_error.h"
#include "flux_policy.h"

void flux_policy_init_default(flux_policy_t *p)
{
    if (!p) return;

    /* READ */
    p->read.speed = FLUX_SPEED_NORMAL;
    p->read.errors = FLUX_ERR_TOLERANT;
    p->read.scan = FLUX_SCAN_ADVANCED;

    p->read.ignore_read_errors = true;
    p->read.fast_error_skip = false;
    p->read.advanced_scan_factor = 100;

    p->read.read_sidechannel = false;
    p->read.dpm = FLUX_DPM_NORMAL;

    p->read.retry.max_revs = 5;
    p->read.retry.max_resyncs = 64;
    p->read.retry.max_retries = 3;
    p->read.retry.settle_ms = 20;

    /* WRITE */
    p->write.speed = FLUX_SPEED_NORMAL;
    p->write.errors = FLUX_ERR_STRICT;
    p->write.max_retries = 3;
    p->write.settle_ms = 20;
    p->write.verify_after_write = true;
    p->write.close_session = true;
    p->write.underrun_protect = true;
}

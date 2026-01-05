#pragma once

#define VFO_TYPE_SIMPLE         (0)
#define VFO_TYPE_FIXED          (1)
#define VFO_TYPE_PID            (2)
#define VFO_TYPE_PID2           (3)
#define VFO_TYPE_SIMPLE2        (4)
#define VFO_TYPE_PID3           (5)
#define VFO_TYPE_EXPERIMANTAL   (9)

#define VFO_TYPE_MAX             VFO_TYPE_PID3          /*Except vfo_experimental */

/**
 * @brief Default VFD type.
 * 
 */
#define VFO_TYPE_DEFAULT        VFO_TYPE_PID3

#define VFO_TYPE_DESC_STR  "0:vfo_simple, 1:vfo_fixed, 2:vfo_pid, 3:vfo_pid2, 4:vfo_simple2, 5:vfo_pid3, 9:vfo_experimental"

#define VFO_GAIN_L_DEFAULT  (0.3f) // 0.2f was too small.  0.4f was just a little too large.
#define VFO_GAIN_H_DEFAULT  (1.f)

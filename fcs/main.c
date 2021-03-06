/*
Copyright (C) 2013 Ben Dyer

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

The PLL and DDR3 setup code is

Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

  Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the
  distribution.

  Neither the name of Texas Instruments Incorporated nor the names of
  its contributors may be used to endorse or promote products derived
  from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <c6x.h>

#include "hardware/board.h"
#include "TRICAL/TRICAL.h"
#include "ukf/cukf.h"
#include "nmpc/cnmpc.h"
#include "ahrs/ahrs.h"
#include "control/control.h"
#include "util/util.h"
#include "stats/stats.h"
#include "exports/exports.h"

int main(void);

#pragma FUNC_NEVER_RETURNS(main);
#pragma FUNC_EXT_CALLED(main);
int main(void) {
    uint32_t core = DNUM & 0xFFu,
             cycles_per_tick = 0;

    /* Perform core-specific initialization */
    cycles_per_tick = fcs_board_init_core();

    if (core == FCS_CORE_PLATFORM) {
        fcs_board_init_platform();
        fcs_exports_init();
    }
    if (core == FCS_CORE_AHRS) {
        fcs_ahrs_init();
    }
    if (core == FCS_CORE_CONTROL) {
        fcs_control_init();
    }

    /*
    Wait until an even number of cycles to avoid excess consumption on the
    first tick
    */
    uint32_t frame = TSCL / cycles_per_tick,
             start_t = frame * cycles_per_tick;
    frame++;

    while (TSCL - start_t < cycles_per_tick);

    while (1) {
        if (core == FCS_CORE_PLATFORM) {
            fcs_board_tick();
        }
        if (core == FCS_CORE_AHRS) {
            fcs_ahrs_tick();
        }
        if (core == FCS_CORE_CONTROL) {
            fcs_control_tick();
        }

        /* Wait until next frame start time */
        start_t = frame * cycles_per_tick;
        frame++;

        fcs_global_counters.main_loop_count[core]++;
        if (TSCL - start_t > fcs_global_counters.main_loop_cycle_max[core]) {
            fcs_global_counters.main_loop_cycle_max[core] = TSCL - start_t;
        }

        if (TSCL - start_t > cycles_per_tick) {
            /*
            Lost an entire frame; abort if it's the AHRS core because that
            should be impossible.
            */
            fcs_assert(core != FCS_CORE_AHRS);
        } else {
            while (TSCL - start_t < cycles_per_tick);
        }
    }

    /*
    Avoid the "unreachable statement" error -- this is valid C99 (main returns
    0 if no return statement), and TI's compiler ignores the error even though
    it only implements sort-of C90.

    return 0;
    */
}

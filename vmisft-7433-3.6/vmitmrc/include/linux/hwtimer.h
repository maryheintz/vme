
/*
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2002 GE Fanuc
    International Copyright Secured.  All Rights Reserved.

-------------------------------------------------------------------------------

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   o Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.
   o Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   o Neither the name of GE Fanuc nor the names of its contributors may be used
     to endorse or promote products derived from this software without specific
     prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

===============================================================================
*/


#ifndef __HWTIMER_H
#define __HWTIMER_H

#include <linux/ioctl.h>


#define TIMER_START                _IO('t', 0x20)
#define TIMER_STOP                 _IO('t', 0x21)
#define TIMER_RATE_SET             _IOW('t', 0x22, int)
#define TIMER_RATE_GET             _IOR('t', 0x23, int)
#define TIMER_LATCH_SYNC_ENABLE    _IO('t', 0x24)
#define TIMER_LATCH_SYNC_DISABLE   _IO('t', 0x25)
#define TIMER_INTERRUPT_WAIT       _IO('t', 0x26)
#define TIMER_INTERRUPT_ENABLE     _IO('t', 0x27)
#define TIMER_INTERRUPT_DISABLE    _IO('t', 0x28)


 /* Rate values supported by the vmitmrf driver.
    DEPRECATED. Do not use these values!!!!!! Use # of kilohertz instead.
  */
typedef enum {
        VMITMRF_RATE__250KHZ = 250,
        VMITMRF_RATE__500KHZ = 500,
        VMITMRF_RATE__1MHZ = 1000,
        VMITMRF_RATE__2MHZ = 2000
} vmirtcf_rate_t;


#endif                          /* __HWTIMER_H */


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


#ifndef __VMITMRF_H
#define __VMITMRF_H


#define VMITMR_NUM_DEVICES              4
#define VMITMR_TCSR1                    0x00
#define VMITMR_TCSR(x)                  (0x0 + (x) - 1)
#define VMITMR_TCSR2                    0x04
#define VMITMR_TMRLCR1                  0x10
#define VMITMR_TMRLCR2                  0x12
#define VMITMR_TMRLCR3                  0x14
#define VMITMR_TMRLCR4                  0x18
#define VMITMR_TMRCCR1                  0x20
#define VMITMR_TMRCCR2                  0x22
#define VMITMR_TMRCCR3                  0x24
#define VMITMR_TMRCCR4                  0x28
#define VMITMR_T1IC                     0x30
#define VMITMR_T2IC                     0x34
#define VMITMR_T3IC                     0x38
#define VMITMR_T4IC                     0x3C
#define VMITMR_TIC(x)                   (VMITMR_T1IC + (((x) - 1) * 4))

#define VMITMR_TCSR1__IRQ_STAT          0x01
#define VMITMR_TCSR1__EN                0x02
#define VMITMR_TCSR1__IRQ_EN            0x04
#define VMITMR_TCSR1__CS                0x18
#define VMITMR_TCSR1__CS__2mhz          0x00
#define VMITMR_TCSR1__CS__1mhz          0x08
#define VMITMR_TCSR1__CS__500khz        0x10
#define VMITMR_TCSR1__CS__250khz        0x18
#define VMITMR_TCSR2__LS                0x01


#endif                          /* __VMITMRF_H */

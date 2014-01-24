
/*
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2003 GE Fanuc
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


#ifndef __VMITMR_7305_H
#define __VMITMR_7305_H


#define VMITMR_NUM_DEVICES              3
#define VMITMR_ENA                      0x00
#define VMITMR_CSR(x)                   (0x04 + (x) - 1)
#define VMITMR_LCR(x)                   (0x08 + (2 * ((x) - 1)))
#define VMITMR_CCR(x)                   (0x10 + (2 * ((x) - 1)))
#define VMITMR_IRQCLR(x)                (0x18 + (x) - 1)

#define VMITMR_ENA__EN(x)               (1 << ((x) - 1))
#define VMITMR_ENA__LATCH               0x04
#define VMITMR_CSR__IRQEN               0x01
#define VMITMR_CSR__CS                  0x06
#define VMITMR_CSR__CS__2mhz            0x00
#define VMITMR_CSR__CS__1mhz            0x02
#define VMITMR_CSR__CS__500khz          0x04
#define VMITMR_CSR__CS__250khz          0x06
#define VMITMR_CSR__IRQSTAT             0x80


#endif                          /* __VMITMR_7305_H */

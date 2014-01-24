
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


#ifndef __VMITMRC_H
#define __VMITMRC_H


#define VMITMR_NUM_DEVICES              3
#define VMITMR_BAR                      4
#define VMITMR_SC(x)                    ((0x10 * ((x) - 1)) + 0x00)
#define VMITMR_LC(x)                    ((0x10 * ((x) - 1)) + 0x04)
#define VMITMR_UC(x)                    ((0x10 * ((x) - 1)) + 0x08)
#define VMITMR_TMR(x)                   ((0x10 * ((x) - 1)) + 0x0c)
#define VMITMR_TWSS                     0x30
#define VMITMR_TEI                      0x34
#define VMITMR_TIS                      0x38

#define VMITMR_TMR__READBACK_16bit      0xd4
#define VMITMR_TMR__READBACK_32bit      0xdc
#define VMITMR_TMR__WRITE_SCALE         0x36
#define VMITMR_TMR__WRITE_LOWER         0x7a
#define VMITMR_TMR__WRITE_UPPER         0xba
#define VMITMR_TWSS__32bit(x)           (0x01 << ((x) - 1))
#define VMITMR_TEI__ENABLE(x)           (0x01 << ((x) - 1))
#define VMITMR_TEI__INTR_MASK(x)        (0x08 << ((x) - 1))
#define VMITMR_TIS__INTR_STAT(x)        (0x01 << ((x) - 1))


#endif                          /* __VMITMRC_H */

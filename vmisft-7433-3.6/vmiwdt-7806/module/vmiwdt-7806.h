
/*
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2002-2004 GE Fanuc
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

#ifndef __VMIWDT_7806_H
#define __VMIWDT_7806_H

#define VMIWDT_CFG                      0x60
#define VMIWDT_CFG__OUT_EN              0x00
#define VMIWDT_CFG__1KHZ                0x00
#define VMIWDT_CFG__INT_DISABLE         0x03

#define VMIWDT_LOCK                     0x68
#define VMIWDT_LOCK__TIMEOUT_MD         0x00
#define VMIWDT_LOCK__ENABLE             0x02
#define VMIWDT_LOCK__DISABLE            0x00

#define VMIWDT_PRELOAD1                 0x00
#define VMIWDT_PRELOAD2                 0x04
#define VMIWDT_PRELOAD__MASK            0x000FFFFF

#define VMIWDT_INT_STATUS               0x08
#define VMIWDT_STATUS__ACTIVE           0x01

#define VMIWDT_RELOAD                   0x0C
#define VMIWDT_RELOAD__WDT_TIMEOUT      0x0200
#define VMIWDT_RELOAD__WDT_RELOAD       0x0100

#define VMIWDT_MIN_TIME_MS              0x00000001 
#define VMIWDT_MAX_TIME_MS              0x00100000 

#endif				/* __VMIWDT_7806_H */

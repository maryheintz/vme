
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


#ifndef __VMITMR_82c54_H
#define __VMITMR_82c54_H


#define VMITMR_NUM_DEVICES              3

#define VMITMR_TIMER(x)                 ((x) - 1)
#define VMITMR_CTL                      0x03

#define VMITMR_CTL__ST                  0xc0
#define VMITMR_CTL__ST__TMR(x)          ((0x10 << (x)) & VMITMR_CTL__ST)
#define VMITMR_CTL__ST__RB              0xc0
#define VMITMR_CTL__RW                  0x30
#define VMITMR_CTL__RW__LATCH           0x00
#define VMITMR_CTL__RW__LSB             0x10
#define VMITMR_CTL__RW__MSB             0x20
#define VMITMR_CTL__RW__LSBMSB          0x30
#define VMITMR_CTL__MODE                0x0e
#define VMITMR_CTL__MODE0               0x00
#define VMITMR_CTL__MODE1               0x02
#define VMITMR_CTL__MODE2               0x04
#define VMITMR_CTL__MODE3               0x06
#define VMITMR_CTL__MODE4               0x08
#define VMITMR_CTL__MODE5               0x0a
#define VMITMR_CTL__BCD                 0x01
#define VMITMR_CTL__RB__CNT             0x20
#define VMITMR_CTL__RB__STAT            0x10
#define VMITMR_CTL__RB__TMR(x)          (1 << (x))

#define PWRMGT_INTSTAT                  0x31
#define PWRMGT_INTCLR                   0x37

#define PWRMGT_INTSTAT__TMR(x)          (0x80 >> ((x) - 1))
#define PWRMGT_INTCLR__TMR(x)           ((1 == (x)) ? 0x40 : (0x40 >> (x)))


#endif                          /* __VMITMR_82c54_H */

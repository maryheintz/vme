
/*
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2005 GE Fanuc
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


#ifndef __VMITMR7326_H
#define __VMITMR7326_H


#define VMITMR_NUM_DEVICES        4

//-------- FPGA Registers ---------//

#define VMITMR_REG_BASE_ADDR      0xffc00000
#define VMITMR_REG_SIZE           0x20

//-------- Timer Registers --------//

#define VMITMR_ENABLE     	    0x00		// Timer Enable

#define VMITMR_TCSR(x)         (0x3 + (x))	// Timer Control Status Registers

#define VMITMR_TMRVAL1            0x08	 	// Timer 1 Value - 16-bit
#define VMITMR_TMRVAL2            0x0A	 	// Timer 1 Value - 16-bit
#define VMITMR_TMRVAL3            0x0C	 	// Timer 1 Value - 32-bit
#define VMITMR_TMRVAL4            0x10	 	// Timer 1 Value - 32-bit

#define VMITMR_TMRCNT1            0x08	 	// Timer 1 Count - 16-bit
#define VMITMR_TMRCNT2            0x0A	 	// Timer 1 Count - 16-bit
#define VMITMR_TMRCNT3            0x0C	 	// Timer 1 Count - 32-bit
#define VMITMR_TMRCNT4            0x10	 	// Timer 1 Count - 32-bit


#define VMITMR_T1IC               0x18		// Clear IRQ 1
#define VMITMR_T2IC               0x19		// Clear IRQ 2
#define VMITMR_T3IC               0x1A		// Clear IRQ 3
#define VMITMR_T4IC               0x1B		// Clear IRQ 4

//----- Register Bits -----//

#define VMITMR_EN(x)    (0x01 << ((x) - 1) )	// ENABLE - Timers Enable

#define VMITMR_TCSR_IRQ_ENABLE     0x01		// TCSR   - IRQ Enable 

#define VMITMR_TCSR__CS            0x06		// TCSR   - Clock Rate - Mask
#define VMITMR_TCSR__CS__2mhz      0x00		// TCSR   - Clock Rate
#define VMITMR_TCSR__CS__1mhz      0x02		// TCSR   - Clock Rate
#define VMITMR_TCSR__CS__500khz    0x04		// TCSR   - Clock Rate
#define VMITMR_TCSR__CS__250khz    0x06		// TCSR   - Clock Rate

#define VMITMR_TCSR_SET_COUNT      0x08		// TCSR   - Timer Count Load

#define VMITMR_TCSR_INT		     0x80		// TCSR   - Interrupt Detected

#endif                          /* __VMITMR7326_H */

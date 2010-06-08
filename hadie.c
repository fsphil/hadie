/* hadie - High Altitude Balloon flight software              */
/*============================================================*/
/* Copyright (C)2010 Philip Heron <phil@sanslogic.co.uk>      */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#include "config.h"
#include <util/delay.h>
#include <avr/interrupt.h>
#include "rtty.h"
#include "rs8.h"

int main(void)
{
	/* Initalise the radio, and let it settle */
	rtx_init();
	_delay_ms(2000);
	
	/* Start interrupts and enter main loop */
	sei();
	
	while(1)
	{
		
	}
	
	return(0);
}


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
#include <util/crc16.h>
#include <avr/interrupt.h>
#include "rtty.h"
#include "rs8.h"

uint16_t gps_CRC16_checksum(char *s)
{
	uint16_t x;
	
	for(x = 0xFFFF; *s; s++)
		x = _crc_xmodem_update(x, (uint8_t) *s);
	
	return(x);
}

int main(void)
{
	/* Initalise the radio, and let it settle */
	rtx_init();
	_delay_ms(2000);
	
	/* Start interrupts and enter main loop */
	sei();
	
	while(1)
	{
		rtx_string_P(PSTR("Hello, World!\n"));
	}
	
	return(0);
}


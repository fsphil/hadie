/* hadie - High Altitude Balloon flight software              */
/*============================================================*/
/* Copyright (C)2010 Philip Heron <phil@sanslogic.co.uk>      */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#include "config.h"
#include <stdint.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>

#define RTTY_BAUD  (300)
#define RTTY_DELAY (1000000 / RTTY_BAUD)

/* MARK = Upper tone, Idle, bit  */
#define TXENABLE (0x04)
#define MARK  (0x02)
#define SPACE (0x01)

/* RTTY speed */
uint16_t rtty_delay = (1000000 / RTTY_BAUD);
//#define RTTY_DELAY rtty_delay

void rtx_baud(int baud)
{
	rtty_delay = 1000000 / baud;
}

inline void rtx_bit(uint8_t b)
{
	PORTB &= ~(MARK | SPACE);
	PORTB |= b;
}

void rtx_byte(uint8_t byte)
{
	int i;
	
	/* Start bit */
	rtx_bit(SPACE);
	_delay_us(RTTY_DELAY);
	
	/* 7 bit */
	for(i = 0; i < 8; i++)
	{
		if(byte & 1 << i) rtx_bit(MARK); else rtx_bit(SPACE);
		_delay_us(RTTY_DELAY);
	}
	
	/* Stop bit 1 */
	rtx_bit(MARK);
	_delay_us(RTTY_DELAY);
	_delay_us(RTTY_DELAY / 2);
}

void rtx_string(char *s)
{
	while(*s != '\0') rtx_byte(*(s++));
}

void rtx_string_P(PGM_P s)
{
	char b;
	while((b = pgm_read_byte(s++)) != '\0') rtx_byte(b);
}

void rtx_data(uint8_t *data, size_t length)
{
	int b;
	for(b = 0; b < length; b++) rtx_byte(data[b]);
}

void rtx_init()
{
	/* We use Port B pins 1, 2 and 3 - MARK by default */
	rtx_bit(MARK);
	PORTB |= TXENABLE;
	DDRB |= MARK | SPACE | TXENABLE;
}


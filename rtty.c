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
#include <io.h>
#include <util/delay.h>
#include <pgmspace.h>

#define RTTY_BAUD  (300)
#define RTTY_DELAY (1000000 / RTTY_BAUD)

/* MARK = Upper tone, Idle, bit  */
#define TXENABLE (0x04)
#define MARK  (0x02)
#define SPACE (0x01)

/* RTTY speed */
uint16_t rtty_delay = (1000000 / RTTY_BAUD);
//#define RTTY_DELAY rtty_delay

void tx_baud(int baud)
{
	rtty_delay = 1000000 / baud;
}

inline void tx_bit(uint8_t b)
{
	PORTB &= ~(MARK | SPACE);
	PORTB |= b;
}

void tx_byte(uint8_t byte)
{
	int i;
	
	/* Start bit */
	tx_bit(SPACE);
	_delay_us(RTTY_DELAY);
	
	/* 7 bit */
	for(i = 0; i < 8; i++)
	{
		if(byte & 1 << i) tx_bit(MARK); else tx_bit(SPACE);
		_delay_us(RTTY_DELAY);
	}
	
	/* Stop bit 1 */
	tx_bit(MARK);
	_delay_us(RTTY_DELAY);
	_delay_us(RTTY_DELAY / 2);
}

void tx_string(char *s)
{
	while(*s != '\0') tx_byte(*(s++));
}

void tx_string_P(PGM_P s)
{
	char b;
	while((b = pgm_read_byte(s++)) != '\0') tx_byte(b);
}

void tx_packet(uint8_t *packet)
{
	int b;
	for(b = 0; b <= PKT_SIZE; b++) tx_byte(packet[b]);
}

void tx_init()
{
	/* We use Port B pins 1, 2 and 3 - MARK by default */
	tx_bit(MARK);
	PORTB |= TXENABLE;
	DDRB |= MARK | SPACE | TXENABLE;
}


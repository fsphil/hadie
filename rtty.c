/* hadie - High Altitude Balloon flight software              */
/*============================================================*/
/* Copyright (C)2010 Philip Heron <phil@sanslogic.co.uk>      */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#include "config.h"
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "rtty.h"

/* MARK = Upper tone, Idle, bit  */
#define TXENABLE (0x04)
#define MARK  (0x02)
#define SPACE (0x01)

#define TXBIT(b) PORTB = (PORTB & ~(MARK | SPACE)) | (b)

volatile uint8_t *txbuf = 0;
volatile uint16_t txlen = 0;

ISR(TIMER0_COMPA_vect)
{
	/* The currently transmitting byte, including framing */
	static uint8_t byte = 0x00;
	static uint8_t bit  = 0x00;
	
	uint8_t b = 0;
	switch(bit++)
	{
	case 0: b = 0; break; /* Start bit */
	case 9: b = 1; break; /* Stop bit */
	case 10: b = 1; TCNT0 += OCR0A >> 1; bit = 0; break; /* Stop bit 0.5 */
	default: b = byte & 1; byte >>= 1; break;
	}
	
	TXBIT(b ? MARK : SPACE);
	
	if(bit == 0 && txlen > 0)
	{
		byte = *(txbuf++);
		txlen--;
	}
}

void rtx_string(char *s)
{
	uint16_t length = strlen(s);
	rtx_wait();
	txbuf = (uint8_t *) s;
	txlen = length;
}

void rtx_string_P(PGM_P s)
{
	//char b;
	//while((b = pgm_read_byte(s++)) != '\0') rtx_byte(b);
}

void rtx_data(uint8_t *data, size_t length)
{
	rtx_wait();
	txbuf = data;
	txlen = length;
}

void rtx_wait()
{
	/* Wait for interrupt driven TX to finish */
	while(txlen > 0);
}

void rtx_init()
{
	/* RTTY is driven by TIMER0 in CTC mode */
	TCCR0A = _BV(WGM01); /* Mode 2, CTC */
	TCCR0B = _BV(CS02) | _BV(CS00); /* prescaler 1024 */
	OCR0A = F_CPU / 1024 / RTTY_BAUD;
	TIMSK0 = _BV(OCIE0A); /* Enable interrupt */
	
	/* We use Port B pins 1, 2 and 3 */
	TXBIT(MARK);
	PORTB |= TXENABLE;
	DDRB |= MARK | SPACE | TXENABLE;
}


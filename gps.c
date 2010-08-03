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
#include <string.h>
#include <stdio.h>
#include <avr/interrupt.h> 
#include "gps.h"

/************* UART RX interrupt handler and buffer *************************/

/* The maximum length of the NMEA string is 82 characters */
#define RXBUFSIZE (82 + 1)

/* Helper to convert uppercase hex ascii character to integer */
#define HTOI(c) (((c) - '0' > '9' ? (c) - 'A' + 10 : (c) - '0') & 0x0F)

/* Received GPGGA lines are copied into this buffer by the interrupt */
static volatile char rxline[RXBUFSIZE];
static volatile char rx_lock = 0;

ISR(USART1_RX_vect)
{
	static char    rx[RXBUFSIZE];
	static uint8_t rx_len = 0; /* Number of characters in the buffer */
	static uint8_t rx_checksum = 0; /* Calculated checksum */
	static uint8_t rx_have_checksum = 0; /* If the string has a checksum */
	static uint8_t checksum; /* The checksum at the end of the string */
	uint8_t b;
	
	/* Read the received character */
	b = UDR1; 
	
	switch(b)
	{     
	case '$': break;
	case '*': rx_have_checksum = 1; return;
	case '\n': 
	case '\r': 
		/* A complete line has been received - is the checksum valid? */
		if(!rx_have_checksum) break;
		if(checksum != rx_checksum) break;
		
		/* Is it a GPGGA message? */
		rx[rx_len] = '\0';
		if(strncmp(rx, "GPGGA", 5) != 0) break;
		
		/* Got a valid GPGGA line, copy it if not locked */
		if(!rx_lock) strncpy((char *) rxline, rx, RXBUFSIZE); 
		
		break;
	
	default:
		if(!rx_have_checksum)
		{
			/* Clear the buffer if full */
			if(rx_len >= RXBUFSIZE) break;
			
			/* Store the new character and update checksum */
			rx[rx_len++] = b;
			rx_checksum ^= b;
		}
		else
		{
			checksum <<= 4;
			checksum  |= HTOI(b);
		}
		
		return;
	}
	
	/* Clear the buffer */
	rx_len = 0;
	rx_checksum = 0;
	rx_have_checksum = 0;
}

/* Like atoi but for fixed point numbers. Processes at most n bytes and
 * returns a pointer the next position in the string. Reads dp decimal
 * places */
int32_t strntofp(const char *s, char **endptr, size_t n, char dp)
{
	int32_t i = 0;
	char neg = 0, fp = 0;
	
	if(n <= 0) goto out;
	
	/* Test for a + or - sign */
	switch(*s)
	{
	case '-': neg = !neg;
	case '+': s++; n--;
	}
	
	/* Read in the number */
	while(*s && n && (!fp || dp))
	{
		char d = *s;
		
		if(d >= '0' && d <= '9')
		{
			/* Add the digit */
			i *= 10;
			i += d - '0';
			
			if(fp) dp--;
		}
		else if(dp > 0 && d == '.') fp = 1;
		else break;
		
		/* Next... */
		s++;
		n--;
	}
	
	while(dp > 0)
	{
		i *= 10;
		dp--;
	}
	
	/* Fix result if it's negative */
	if(neg) i = -i;
	
out:
	/* Set the end pointer if needed */
	if(endptr) *endptr = (char *) s;
	
	return(i);
}

int32_t strntoi(const char *s, char **endptr, size_t n)
{
	return(strntofp(s, endptr, n, 0));
}

char *gps_field(char *s, int f)
{
	while(*s && f > 0) if(*(s++) == ',') f--;
	if(f == 0) return(s);
	return(NULL);
}

char gps_parse(gpsfix_t *gps)
{
	char i;
	
	memset(gps, 0, sizeof(gpsfix_t));
	rx_lock = 1;
	
	for(i = 0; i < 10; i++)
	{
		char *r = gps_field((char *) rxline, i);
		if(!r)
		{
			rx_lock = 0;
			return(-1);
		}
		
		switch(i)
		{
		case 1: /* Fix Time */
			gps->hour   = strntoi(r, &r, 2);
			gps->minute = strntoi(r, &r, 2);
			gps->second = strntoi(r, &r, 2);
			break;
		
		case 2: /* Latitude */
			gps->latitude_i = strntoi(r, &r, 2);
			gps->latitude_f = strntofp(r, &r, 7, 4) * 100 / 60;
			break;
		
		case 3: /* Latitude hemisphere */
			if(*r == 'S') gps->latitude_i = -gps->latitude_i;
			break;
		
		case 4: /* Longitude */
			gps->longitude_i = strntoi(r, &r, 3);
			gps->longitude_f = strntofp(r, &r, 7, 4) * 100 / 60;
			break;
		
		case 5: /* Longitude hemisphere */
			if(*r == 'W') gps->longitude_i = -gps->longitude_i;
			break;
		
		case 6: /* Fix quality */
			gps->fix = strntoi(r, NULL, 3);
			break;
		
		case 7: /* Satellites */
			gps->sats = strntoi(r, NULL, 3);
			break;
		
		case 9: /* Altitude */
			gps->altitude = strntoi(r, NULL, 10);
			break;
		}
	}
	
	rx_lock = 0;
	
	return(0);
}

void gps_init()
{
	rxline[0] = '\0';
	
	/* Do UART1 initialisation, 38400 baud @ 7.3728 MHz */
	UBRR1H = 0;
	UBRR1L = 11;
	
	/* Enable RX, TX and RX interrupt */
	UCSR1B = (1 << RXEN1) | (1 << TXEN1) | (1 << RXCIE1);
	
	/* 8-bit, no parity and 1 stop bit */
	UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
}


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

#ifdef UBLOX5
#include <avr/pgmspace.h>
#endif

/************* UART RX interrupt handler and buffer *************************/

/* Timeout counter */
volatile static uint8_t timeout_clk = 0;

inline void gps_tick(void)
{
	if(timeout_clk) timeout_clk--;
}

/* The maximum length of the NMEA string is 82 characters */
#define RXBUFSIZE (82 + 1)

/* Helper to convert uppercase hex ascii character to integer */
#define HTOI(c) (((c) - '0' > '9' ? (c) - 'A' + 10 : (c) - '0') & 0x0F)

#ifdef UBLOX5

#define UBXBUFSIZE (16)

static volatile uint8_t ubx_rx[UBXBUFSIZE];
static volatile uint8_t ubx_rx_len = 0;
static volatile uint8_t ubx_want_packet = 0;

static inline void isr_ubx(uint8_t b)
{
	static uint8_t rx[UBXBUFSIZE];
	static uint8_t rx_len = 0;
	static uint16_t pkt_len = 8;
	static uint8_t ck_a = 0, ck_b = 0;
	
	rx[rx_len++] = b;
	if(rx_len < 2) return;
	
	/* Should have received the sync pattern */
	if(rx_len == 2)
	{
		if(rx[0] != 0xB5 || rx[1] != 0x62)
		{
			/* Nope */
			rx[0] = rx[1];
			rx_len = 1;
		}
		
		return;
	}
	
	/* Update the checksum */
	if(rx_len <= pkt_len - 2) ck_b += ck_a += b;
	
	/* Add the payload data length to the packet size */
	if(rx_len == 6)
	{
		pkt_len += (rx[5] << 8) + rx[4];
		
		/* Drop the packet if it's going to be too big for the buffer */
		if(pkt_len > UBXBUFSIZE) goto clear;
	}
	
	/* Wait until the complete packet has been received */
	if(rx_len < pkt_len) return;
	
	/* The entire packet should now be in the buffer, test the checksum */
	if(rx[rx_len - 2] != ck_a) goto clear;
	if(rx[rx_len - 1] != ck_b) goto clear;
	
	/* A valid packet has been received - copy it and set the flag */
	memcpy((void *) ubx_rx, rx, rx_len);
	ubx_rx_len = rx_len;
	ubx_want_packet = 0;
	
	/* Done with this packet */
clear:
	rx_len = 0;
	pkt_len = 8;
	ck_a = ck_b = 0;
}

#endif

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
	
#ifdef UBLOX5
	if(ubx_want_packet) isr_ubx(b);
#endif
	
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

#ifdef UBLOX5
static void tx_byte(uint8_t b)
{
	/* Wait for empty transmit buffer */
	while(!(UCSR1A & (1 << UDRE1)));
	
	/* Put data into buffer, sends the data */
	UDR1 = b;
}
#endif

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
			gps->latitude_h = (*r == 'S' ? 'S' : 'N');
			break;
		
		case 4: /* Longitude */
			gps->longitude_i = strntoi(r, &r, 3);
			gps->longitude_f = strntofp(r, &r, 7, 4) * 100 / 60;
			break;
		
		case 5: /* Longitude hemisphere */
			gps->longitude_h = (*r == 'W' ? 'W' : 'E');
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

#ifdef UBLOX5
/* From the FSA03 guide on the ukhas wiki: *
 * http://ukhas.org.uk/guides:falcom_fsa03 */

PROGMEM const uint8_t ubx_setnav[] = {
0xB5,0x62,0x06,0x24,0x24,0x00,0xFF,0xFF,0x06,0x03,0x00,0x00,0x00,0x00,0x10,0x27,
0x00,0x00,0x05,0x00,0xFA,0x00,0xFA,0x00,0x64,0x00,0x2C,0x01,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x16,0xDC
};

PROGMEM const uint8_t ubx_setnav_ack[] = { 0x05,0x01,0x02,0x00,0x06,0x24 };

char gps_ubx_init(void)
{
	uint8_t i;
	
	/* Send the prepared command */
	for(i = 0; i < sizeof(ubx_setnav); i++)
		tx_byte(pgm_read_byte(&ubx_setnav[i]));
	
	/* Wait for a response */
	ubx_want_packet = 1;
	
	timeout_clk = 255; /* 0.85 seconds at 300hz */
	while(timeout_clk)
	{
		if(ubx_want_packet == 1) continue;
		
		/* Got a packet... the ACK for the command? */
		if(!memcmp_P((void *) &ubx_rx[2], ubx_setnav_ack, sizeof(ubx_setnav_ack)))
		{
			/* GPS device acknowledged the command */
			return(0);
		}
		
		ubx_want_packet = 1;
	}
	
	/* Timeout waiting for ACK packet */
	ubx_want_packet = 0;
	
	return(-1);
}
#endif

void gps_init(void)
{
	rxline[0] = '\0';
	
	/* Do UART1 initialisation, 9600 baud @ 7.3728 MHz */
	UBRR1H = 0;
	UBRR1L = 47;
	
	/* Enable RX, TX and RX interrupt */
	UCSR1B = (1 << RXEN1) | (1 << TXEN1) | (1 << RXCIE1);
	
	/* 8-bit, no parity and 1 stop bit */
	UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
}


/* hadie - High Altitude Balloon flight software              */
/*============================================================*/
/* Copyright (C)2010 Philip Heron <phil@sanslogic.co.uk>      */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

/* Interface to the C328 UART camera */

#include "config.h"
#include <stdint.h>
#include <string.h>
#include <avr/io.h> 
#include "c328.h"

/* >10ms timeout at 300 hz */
#define CMD_TIMEOUT (20)

/* Wait longer for the camera to take the image and return DATA response */
#define PIC_TIMEOUT (200)

#define RXREADY (UCSR0A & (1 << RXC0))

/* Receive buffer */
#define RXBUF_LEN (256)
uint8_t rxbuf[RXBUF_LEN];
uint16_t rxbuf_len = 0;

/* Expected package size */
static uint16_t pkg_len = 64; /* Default is 64 according to datasheet */

/* Timeout counter */
volatile static uint8_t timeout_clk = 0;

inline void c3_tick(void)
{
	if(timeout_clk) timeout_clk--;
}

static void tx_byte(uint8_t b)
{
	/* Wait for empty transmit buffer */
	while(!(UCSR0A & (1 << UDRE0)));
	
	/* Put data into buffer, sends the data */
	UDR0 = b;
}

static uint8_t c3_rx(uint8_t timeout)
{
	rxbuf_len = 0;
	
	timeout_clk = timeout;
	while(timeout_clk)
	{
		if(!RXREADY) continue;
		rxbuf[rxbuf_len++] = UDR0;
		if(rxbuf_len == 6) break;
	}
	
	if(rxbuf_len != 6) return(0); /* Timeout or incomplete response */
	if(rxbuf[0] != 0xAA) return(0); /* All responses should begin 0xAA */
	
	/* Return the received command ID */
	return(rxbuf[1]);
}

static void c3_tx(uint8_t cmd, uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4)
{
	tx_byte(0xAA);
	tx_byte(cmd);
	tx_byte(a1);
	tx_byte(a2);
	tx_byte(a3);
	tx_byte(a4);
}

static char c3_cmd(uint8_t cmd, uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4)
{
	uint8_t r;
	
	c3_tx(cmd, a1, a2, a3, a4);
	r = c3_rx(CMD_TIMEOUT);
	
	/* Did we get an ACK for this command? */
	if(r != CMD_ACK || rxbuf[2] != cmd) return(-1);
	
	return(0);
}

void c3_init(void)
{
	/* Do UART initialisation, port 0 @ 14.4k baud for 7.3728 MHz clock */
	UBRR0H = 0;
	UBRR0L = 31;
	
	/* Enable TX & RX */
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);
	
	/* 8-bit, no parity and 1 stop bit */
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

char c3_sync(void)
{
	char i;
	
	/* Send the SYNC command until the camera responds, up to 60 */
	for(i = 0; i < 60; i++)
	{
		/* Send the sync and wait for an ACK */
		if(c3_cmd(CMD_SYNC, 0, 0, 0, 0) != 0) continue;
		
		/* ACK should be followed by a SYNC */
		if(c3_rx(CMD_TIMEOUT) != CMD_SYNC) continue;
		
		/* ACK the SYNC and return success code */
		c3_tx(CMD_ACK, CMD_SYNC, 0, 0, 0);
		
		return(0);
	}
	
	/* If we got here, the camera failed to sync. Panic */
	return(-1);
}

char c3_setup(uint8_t ct, uint8_t rr, uint8_t jr)
{
	return(c3_cmd(CMD_INIT, 0, ct, rr, jr));
}

char c3_set_package_size(uint16_t s)
{
	char r;
	
	if(s > RXBUF_LEN) return(-1);
	
	r = c3_cmd(CMD_SET_PKG_SIZE, 0x08, s & 0xFF, s >> 8, 0);
	
	if(r == 0) pkg_len = s;
	
	return(r);
}

char c3_snapshot(uint8_t st, uint16_t skip_frame)
{
	return(c3_cmd(CMD_SNAPSHOT, st, skip_frame & 0xFF, skip_frame >> 8, 0));
}

char c3_get_picture(uint8_t pt, uint16_t *length)
{
	/* Send the command */
	if(c3_cmd(CMD_GET_PICTURE, pt, 0, 0, 0) != 0) return(-1);
	
	/* The camera should now send a DATA message */
	if(c3_rx(PIC_TIMEOUT) != CMD_DATA) return(-1);
	
	/* Get the file size from the DATA args */
	*length = rxbuf[3] + (rxbuf[4] << 8);
	
	return(0);
}

char c3_get_package(uint16_t id, uint8_t **dst, uint16_t *length)
{
	uint8_t checksum;
	volatile uint16_t s;
	/* s is volatile to work around an apparent bug in avr-gcc --
	 * discovered by ms7821 in #highaltitude */
	
	rxbuf_len = 0;
	checksum = 0;
	s = pkg_len;
	
	/* Get the package by sending an ACK */
	c3_tx(CMD_ACK, 0, 0, id & 0xFF, id >> 8);
	
	/* The camera should immediatly start returning data */
	timeout_clk = CMD_TIMEOUT;
	while(timeout_clk && rxbuf_len < s)
	{
		if(!RXREADY) continue;
		
		/* Read the byte and update checksum */
		rxbuf[rxbuf_len] = UDR0;
		checksum += rxbuf[rxbuf_len++];
		
		if(rxbuf_len == 4)
		{
			/* Get the actual length of the package */
			s = rxbuf[2] + (rxbuf[3] << 8) + 6;
			if(s > pkg_len) return(-1);
		}
		
		timeout_clk = CMD_TIMEOUT;
	}
	
	/* Test for timeout or incomplete package */
	if(rxbuf_len != s) return(-2);
	
	/* Fix and test checksum */
	checksum -= rxbuf[rxbuf_len - 2];
	if(checksum != rxbuf[rxbuf_len - 2]) return(-3);
	
	/* All done */
	*dst = rxbuf;
	*length = rxbuf_len;
	
	return(0);
}

char c3_finish_picture(void)
{
	c3_tx(CMD_ACK, 0, 0, 0xF0, 0xF0);
	return(0);
}

/****************** Simple interface ******************/

static uint16_t image_len;
static uint16_t image_read;

static uint8_t *package;
static uint16_t package_len;
static uint16_t package_id;

char c3_open(uint8_t jr)
{
	/* Open, setup and take the image */
	if(c3_sync() != 0) return(-1);
	if(c3_setup(CT_JPEG, 0, jr) != 0) return(-2);
	if(c3_set_package_size(RXBUF_LEN) != 0) return(-3);
	if(c3_snapshot(ST_JPEG, 0) != 0) return(-4);
	if(c3_get_picture(PT_SNAPSHOT, &image_len) != 0) return(-5);
	
	image_read = 0;
	package = NULL;
	package_len = 0;
	package_id = 0;
	
	return(0);
}

char c3_close(void)
{
	c3_finish_picture();
	return(0);
}

uint16_t c3_read(uint8_t *ptr, uint16_t length)
{
	uint16_t r; /* Number of bytes left to read */
	
	/* Don't read past the end of the image */
	r = image_len - image_read;
	if(length > r) length = r;
	
	r = length;
	while(r)
	{
		if(package_len == 0)
		{
			char i = c3_get_package(package_id++, &package, &package_len);
			if(i != 0) return(length - r);
			
			/* Skip the package headers and checksum */
			package += 4;
			package_len -= 6;
		}
		else
		{
			uint16_t b = r;
			if(b > package_len) b = package_len;
			
			if(ptr)
			{
				memcpy(ptr, package, b);
				ptr += b;
			}
			
			package     += b;
			package_len -= b;
			
			r -= b;
			
			image_read += b;
		}
	}
	
	return(length);
}

uint16_t c3_filesize(void)
{
	return(image_len);
}

char c3_eof(void)
{
	return(image_read >= image_len);
}


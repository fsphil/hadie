/* hadie - High Altitude Balloon flight software              */
/*============================================================*/
/* Copyright (C)2010 Philip Heron <phil@sanslogic.co.uk>      */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#include <stdint.h>

#ifndef INC_SSDV_H
#define INC_SSDV_H

#define SSDV_ERROR       (-1)
#define SSDV_OK          (0)
#define SSDV_FEED_ME     (1)
#define SSDV_HAVE_PACKET (2)
#define SSDV_BUFFER_FULL (3)
#define SSDV_EOI         (4)

/* Packet details */
#define SSDV_PKT_SIZE         (0x100)
#define SSDV_PKT_SIZE_HEADER  (0x0B)
#define SSDV_PKT_SIZE_RSCODES (0x20)
#define SSDV_PKT_SIZE_PAYLOAD (SSDV_PKT_SIZE - SSDV_PKT_SIZE_HEADER - SSDV_PKT_SIZE_RSCODES)

#define HBUFF_LEN (16)
#define COMPONENTS (3)

typedef struct
{
	/* Image information */
	uint16_t width;
	uint16_t height;
	uint8_t image_id;
	uint16_t packet_id;
	uint16_t mcu_id;
	uint16_t mcu_count;
	uint16_t packet_mcu_id;
	uint16_t packet_mcu_offset;
	
	/* Source buffer */
	uint8_t *inp;      /* Pointer to next input byte                    */
	size_t in_len;     /* Number of input bytes remaining               */
	size_t in_skip;    /* Number of input bytes to skip                 */
	
	/* Source bits */
	uint32_t workbits; /* Input bits currently being worked on          */
	uint8_t worklen;   /* Number of bits in the input bit buffer        */
	
	/* JPEG / Packet output buffer */
	uint8_t *out;      /* Pointer to the beginning of the output buffer */
	uint8_t *outp;     /* Pointer to the next output byte               */
	size_t out_len;    /* Number of output bytes remaining              */
	
	/* Output bits */
	uint32_t outbits;  /* Output bit buffer                             */
	uint8_t outlen;    /* Number of bits in the output bit buffer       */
	
	/* JPEG decoder state */
	enum {
		S_MARKER = 0,
		S_MARKER_LEN,
		S_MARKER_DATA,
		S_HUFF,
		S_INT,
		S_EOI
	} state;
	uint16_t marker;    /* Current marker                               */
	uint16_t marker_len; /* Length of data following marker             */
	uint8_t *marker_data; /* Where to copy marker data too              */
	uint16_t marker_data_len; /* How much is there                      */
	unsigned char component; /* 0 = Y, 1 = Cb, 2 = Cr                   */
	unsigned char mcupart;   /* 0-3 = Y, 4 = Cb, 5 = Cr                 */
	unsigned char acpart;   /* 0 - 64; 0 = DC, 1 - 64 = AC              */
	int dc[COMPONENTS]; /* DC value for each component                  */
	uint8_t acrle;      /* RLE value for current AC value               */
	unsigned char needbits; /* Number of bits needed to decode integer  */
	
	/* Small buffer for reading SOF0 and SOS header data into */
	uint8_t hbuff[HBUFF_LEN];
	
} ssdv_t;

/* Encoding */
extern char ssdv_enc_init(ssdv_t *s, uint8_t image_id);
extern char ssdv_enc_set_buffer(ssdv_t *s, uint8_t *buffer);
extern char ssdv_enc_get_packet(ssdv_t *s);
extern char ssdv_enc_feed(ssdv_t *s, uint8_t *buffer, size_t length);

#endif


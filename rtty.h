/* hadie - High Altitude Balloon flight software              */
/*============================================================*/
/* Copyright (C)2010 Philip Heron <phil@sanslogic.co.uk>      */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#ifndef INC_RTTY_H
#define INC_RTTY_H

#include <stdint.h>
#include <pgmspace.h>

extern void rtx_baud(int baud);
extern void rtx_string(char *s);
extern void rtx_string_P(PGM_P s);
extern void rtx_packet(uint8_t *packet);
extern void rtx_init();

#endif

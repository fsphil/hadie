/* hadie - High Altitude Balloon flight software              */
/*============================================================*/
/* Copyright (C)2010 Philip Heron <phil@sanslogic.co.uk>      */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#ifndef INC_C328_H
#define INC_C328_H

/* Commands */
#define CMD_INIT                0x01
#define CMD_GET_PICTURE         0x04
#define CMD_SNAPSHOT            0x05
#define CMD_SET_PKG_SIZE        0x06
#define CMD_SET_BAUDRATE        0x07
#define CMD_RESET               0x08
#define CMD_POWER_OFF           0x09
#define CMD_DATA                0x0A
#define CMD_SYNC                0x0D
#define CMD_ACK                 0x0E
#define CMD_NAK                 0x0F
#define CMD_SET_LIGHT_FREQUENCY 0x13

/* Colour Types */
#define CT_2BIT_GRAY    0x01
#define CT_4BIT_GRAY    0x02
#define CT_8BIT_GRAY    0x03
#define CT_12BIT_COLOUR 0x05
#define CT_16BIT_COLOUR 0x06
#define CT_JPEG         0x07

/* Preview Resolutions */
#define PR_80x60   0x01
#define PR_160x120 0x03

/* Snapshot resolutions */
#define SR_80x64   0x01
#define SR_160x128 0x03
#define SR_320x240 0x05
#define SR_640x480 0x07

/* Picture Type */
#define PT_SNAPSHOT     0x01
#define PT_PREVIEW      0x02
#define PT_JPEG_PREVIEW 0x03

/* Snapshot Type */
#define ST_JPEG 0x00
#define ST_RAW  0x01

/* Baudrates (0xXXYY, XX = First Divider, YY = Second Divider) */
#define BR_7200   0xFF01
#define BR_9600   0xBF01
#define BR_14400  0x7F01
#define BR_19200  0x5F01
#define BR_28800  0x3F01
#define BR_38400  0x2F01
#define BR_57600  0x1F01
#define BR_115200 0x0F01
#define BR_921600 0x0101 /* Undocumented */

/* Light Frequencies */
#define LF_50HZ 0x00
#define LF_60HZ 0x01

/* Errors */
#define ERR_PICTURE_TYPE_ERROR              0x01
#define ERR_PICTURE_UP_SCALE                0x02
#define ERR_PICTURE_SCALE_ERROR             0x03
#define ERR_UNEXPECTED_REPLY                0x04
#define ERR_SEND_PICTURE_TIMEOUT            0x05
#define ERR_UNEXPECTED_COMMAND              0x06
#define ERR_SRAM_JPEG_TYPE_ERROR            0x07
#define ERR_SRAM_JPEG_SIZE_ERROR            0x08
#define ERR_PICTURE_FORMAT_ERROR            0x09
#define ERR_PICTURE_SIZE_ERROR              0x0A
#define ERR_PARAMETER_ERROR                 0x0B
#define ERR_SEND_REGISTER_TIMEOUT           0x0C
#define ERR_COMMAND_ID_ERROR                0x0D
#define ERR_PICTURE_NOT_READY               0x0F
#define ERR_TRANSFER_PACKAGE_NUMBER_ERROR   0x10
#define ERR_SET_TRANSFER_PACKAGE_SIZE_WRONG 0x11
#define ERR_COMMAND_HEADER_ERROR            0xF0
#define ERR_COMMAND_LENGTH_ERROR            0xF1
#define ERR_SEND_PICTURE_ERROR              0xF5
#define ERR_SEND_COMMAND_ERROR              0xFF

extern void inline c3_tick(void);

extern void c3_init(void);
extern char c3_sync(void);

extern char c3_setup(uint8_t ct, uint8_t rr, uint8_t jr);
extern char c3_set_package_size(uint16_t s);
extern char c3_snapshot(uint8_t st, uint16_t skip_frame);
extern char c3_get_picture(uint8_t pt, uint16_t *length);
extern char c3_get_package(uint16_t id, uint8_t **dst, uint16_t *length);
extern char c3_finish_picture(void);

extern char c3_open(uint8_t jr);
extern char c3_close(void);
extern uint16_t c3_read(uint8_t *ptr, uint16_t length);
extern uint16_t c3_filesize(void);
extern char c3_eof(void);

#endif

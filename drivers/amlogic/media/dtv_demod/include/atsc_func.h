/*
 * drivers/amlogic/media/dtv_demod/include/atsc_func.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __ATSC_FUNC_H__
#define __ATSC_FUNC_H__

/*#include "demod_func.h"*/

enum atsc_state_machine {
	IDLE = 0x20,
	CR_LOCK = 0x50,
	CR_PEAK_LOCK = 0x62,
	ATSC_SYNC_LOCK = 0x70,
	ATSC_LOCK = 0x76
};

enum atsc_performance {
	TASK4_TASK5 = 1,
	AWGN,
	TASK8_R22,
};

enum ATSC_SYS_STA {
	ATSC_SYS_STA_RST_STATE1,
	ATSC_SYS_STA_RST_STATE2,
	ATSC_SYS_STA_DETECT_CFO_USE_PILOT,
	ATSC_SYS_STA_DETECT_SFO_USE_PN = 5,
	ATSC_SYS_STA_DETECT_PN_IN_EQ_OUT,
	ATSC_SYS_STA_WORKING,
	ATSC_SYS_STA_QAM_WORK_MODE,
	ATSC_SYS_STA_NUM
};

#define LOCK	1
#define UNLOCK	0
#define CFO_OK	1
#define CFO_FAIL 0
#define Dagc_Open 1
#define Dagc_Close 0
#define Atsc_BandWidth	(6000)

/* atsc */

int atsc_set_ch(struct aml_demod_sta *demod_sta,
		/*struct aml_demod_i2c *demod_i2c,*/
		struct aml_demod_atsc *demod_atsc);
int check_atsc_fsm_status(void);

void atsc_write_reg(unsigned int reg_addr, unsigned int reg_data);

unsigned int atsc_read_reg(unsigned int reg_addr);
extern void atsc_write_reg_v4(unsigned int addr, unsigned int data);
void atsc_write_reg_bits_v4(u32 addr, const u32 data, const u32 start, const u32 len);
extern unsigned int atsc_read_reg_v4(unsigned int addr);

unsigned int atsc_read_iqr_reg(void);

/*int atsc_qam_set(fe_modulation_t mode);*/
int atsc_qam_set(enum fe_modulation mode);


void qam_initial(int qam_id);

void set_cr_ck_rate(void);

void atsc_reset(void);

int atsc_find(unsigned int data, unsigned int *ptable, int len);

int atsc_read_snr(void);

unsigned int atsc_read_ser(void);

void atsc_thread(void);

void atsc_set_performance_register(int flag, int init);

int snr_avg_100_times(void);

void atsc_set_version(int version);
void atsc_check_fsm_status(void);

#endif

/*
 * drivers/amlogic/dvb/demux/aml_key.c
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include <linux/wait.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "aml_key.h"
#include "sc2_demux/dvb_reg.h"
#include "key_reg.h"
#include "dmx_log.h"

#define KTE_MAX				128
#define KTE_INVALID			0
#define KTE_VALID			1
#define REE_SUCCESS			0
#define KTE_KTE_MASK		0x7F

#define KTE_PENDING          (1)
#define KTE_MODE_HOST        (3)
#define KTE_CLEAN_KTE        (1)

#define KTE_STATUS_MASK      (3)
#define MKL_STS_OK           (0)

#define MKL_USER_LOC_DEC     (8)
#define MKL_USER_NETWORK     (9)
#define MKL_USER_LOC_ENC     (10)
/* key algorithm */
#define MKL_USAGE_AES        (0)
#define MKL_USAGE_TDES       (1)
#define MKL_USAGE_DES        (2)
#define MKL_USAGE_NDL        (7)
#define MKL_USAGE_ND         (8)
#define MKL_USAGE_CSA3       (9)
#define MKL_USAGE_CSA2       (10)
#define MKL_USAGE_HMAC       (13)

struct key_table_s {
	u32 flag;		//0:invalid, 1:valid
	int key_userid;
	int key_algo;
};

static struct key_table_s key_table[KTE_MAX];
static struct mutex mutex;

#define dprint_i(fmt, args...)   \
	dprintk(LOG_DBG, debug_key, fmt, ## args)
#define dprint(fmt, args...)    \
	dprintk(LOG_ERROR, debug_key, "key:" fmt, ## args)
#define pr_dbg(fmt, args...)    \
	dprintk(LOG_DBG, debug_key, "key:" fmt, ## args)

MODULE_PARM_DESC(debug_key, "\n\t\t Enable key debug information");
static int debug_key;
module_param(debug_key, int, 0644);

static struct class *dmx_key_class;
static int major_key;

static int kt_init(void)
{
	u32 i;

	for (i = 0; i < KTE_MAX; i++)
		key_table[i].flag = KTE_INVALID;

	return REE_SUCCESS;
}

static int kt_alloc(u32 *kte, struct key_config *config)
{
	int res = REE_SUCCESS;
	u32 i;

	if (!kte)
		return -1;

	pr_dbg("%s user_id:%d, key_alog:%d\n", __func__,
	       config->key_userid, config->key_algo);
	for (i = 0; i < KTE_MAX; i++) {
		if (key_table[i].flag == KTE_INVALID) {
			switch (config->key_userid) {
			case DSC_LOC_DEC:
				key_table[i].key_userid = MKL_USER_LOC_DEC;
				break;
			case DSC_NETWORK:
				key_table[i].key_userid = MKL_USER_NETWORK;
				break;
			case DSC_LOC_ENC:
				key_table[i].key_userid = MKL_USER_LOC_ENC;
				break;
			default:
				dprint("%s, %d invalid user id\n",
				       __func__, __LINE__);
				return -1;
			}
			switch (config->key_algo) {
			case KEY_ALGO_AES:
				key_table[i].key_algo = MKL_USAGE_AES;
				break;
			case KEY_ALGO_TDES:
				key_table[i].key_algo = MKL_USAGE_TDES;
				break;
			case KEY_ALGO_DES:
				key_table[i].key_algo = MKL_USAGE_DES;
				break;
			case KEY_ALGO_CSA2:
				key_table[i].key_algo = MKL_USAGE_CSA2;
				break;
			case KEY_ALGO_CSA3:
				key_table[i].key_algo = MKL_USAGE_CSA3;
				break;
			case KEY_ALGO_NDL:
				key_table[i].key_algo = MKL_USAGE_NDL;
				break;
			case KEY_ALGO_ND:
				key_table[i].key_algo = MKL_USAGE_ND;
				break;
			default:
				dprint("%s, %d invalid algo\n",
				       __func__, __LINE__);
				return -1;
			}
			key_table[i].flag = KTE_VALID;
			*kte = i;
//                      pr_dbg("%s get key_index:%d\n", __func__, i);
			break;
		}
	}

	if (i == KTE_MAX) {
//              dprint("%s, %d key slot full\n", __func__, __LINE__);
		res = -1;
	} else {
		res = REE_SUCCESS;
	}
	return res;
}

static int kt_set(u32 kte, unsigned char key[32], unsigned int key_len)
{
	int res = REE_SUCCESS;
	u32 KT_KEY0, KT_KEY1, KT_KEY2, KT_KEY3;
	u32 key0 = 0, key1 = 0, key2 = 0, key3 = 0;
	int user_id = 0;
	int algo = 0;
	int en_decrypt = 0;

//	int i = 0;
//	dprint_i("kte:%d, len:%d key:\n", kte, key_len);
//	for (i = 0; i < key_len; i++)
//		dprint_i("0x%0x ", key[i]);
//	dprint_i("\n");

	if ((kte & (~KTE_KTE_MASK)) != 0) {
//              dprint("%s, %d kte invalid\n", __func__, __LINE__);
		return -1;
	}
	if (key_table[kte].flag != KTE_VALID) {
//              dprint("%s, %d kte flag invalid\n", __func__, __LINE__);
		return -1;
	}
	/*KEY_FROM_REE_HOST: */
	KT_KEY0 = KT_REE_KEY0;
	KT_KEY1 = KT_REE_KEY1;
	KT_KEY2 = KT_REE_KEY2;
	KT_KEY3 = KT_REE_KEY3;

	if (key_len >= 4)
		memcpy((void *)&key0, &key[0], 4);
	if (key_len >= 8)
		memcpy((void *)&key1, &key[4], 4);
	if (key_len >= 12)
		memcpy((void *)&key2, &key[8], 4);
	if (key_len >= 16)
		memcpy((void *)&key3, &key[12], 4);

	user_id = key_table[kte].key_userid;
	if (user_id == MKL_USER_LOC_DEC || user_id == MKL_USER_NETWORK)
		en_decrypt = 2;
	else
		en_decrypt = 1;

	algo = key_table[kte].key_algo;
	//TODO: timeout instead of return error
	if (READ_CBUS_REG(KT_REE_RDY) == 0) {
		dprint("%s, %d not ready\n", __func__, __LINE__);
		return -1;
	}

	WRITE_CBUS_REG(KT_KEY0, key0);
	WRITE_CBUS_REG(KT_KEY1, key1);
	WRITE_CBUS_REG(KT_KEY2, key2);
	WRITE_CBUS_REG(KT_KEY3, key3);
	WRITE_CBUS_REG(KT_REE_CFG, (KTE_PENDING << KTE_PENDING_OFFSET)
		       | (KTE_MODE_HOST << KTE_MODE_OFFSET)
		       | (en_decrypt << KTE_FLAG_OFFSET)
		       | (algo << KTE_KEYALGO_OFFSET)
		       | (user_id << KTE_USERID_OFFSET)
		       | (kte << KTE_KTE_OFFSET)
		       | (0 << KTE_TEE_PRIV_OFFSET)
		       | (0 << KTE_LEVEL_OFFSET));
	do {
		res = READ_CBUS_REG(KT_REE_CFG);
	} while (res & (KTE_PENDING << KTE_PENDING_OFFSET));

	pr_dbg("KT_CFG[0x%08x]=0x%08x\n", KT_REE_CFG, res);
	WRITE_CBUS_REG(KT_REE_RDY, 1);
	if (((res >> KTE_STATUS_OFFSET) & KTE_STATUS_MASK) == MKL_STS_OK) {
		res = REE_SUCCESS;
	} else {
		dprint("%s, fail\n", __func__);
		res = -1;
	}
	return res;
}

int kt_free(int kte)
{
	int res = REE_SUCCESS;

//      pr_dbg("%s kte:%d enter\n", __func__, kte);
	if ((kte & (~KTE_KTE_MASK)) != 0) {
//              dprint("%s, %d kte invalid\n", __func__, __LINE__);
		return -1;
	}
	if (READ_CBUS_REG(KT_REE_RDY) == 0) {
//              dprint("%s, %d not ready\n", __func__, __LINE__);
		return -1;
	}
	WRITE_CBUS_REG(KT_REE_CFG, (KTE_PENDING << KTE_PENDING_OFFSET)
		       | (KTE_CLEAN_KTE << KTE_CLEAN_OFFSET)
		       | (KTE_MODE_HOST << KTE_MODE_OFFSET)
		       | (kte << KTE_KTE_OFFSET));
	do {
		res = READ_CBUS_REG(KT_REE_CFG);
	} while (res & (KTE_PENDING << KTE_PENDING_OFFSET));
	pr_dbg("KT_REE_CFG=0x%08x\n", res);

	WRITE_CBUS_REG(KT_REE_RDY, 1);
	if (((res >> KTE_STATUS_OFFSET) & KTE_STATUS_MASK) == MKL_STS_OK) {
		key_table[kte].flag = KTE_INVALID;
		res = REE_SUCCESS;
	} else {
//              dprint("%s, clean key index fail\n", __func__);
		res = -1;
	}
	return res;
}

static int usercopy(struct file *file,
		    unsigned int cmd, unsigned long arg,
		    int (*func)(struct file *file,
				unsigned int cmd, void *arg))
{
	char sbuf[128];
	void *mbuf = NULL;
	void *parg = NULL;
	int err = -EINVAL;

	/*  Copy arguments into temp kernel buffer  */
	switch (_IOC_DIR(cmd)) {
	case _IOC_NONE:
		/*
		 * For this command, the pointer is actually an integer
		 * argument.
		 */
		parg = (void *)arg;
		break;
	case _IOC_READ:	/* some v4l ioctls are marked wrong ... */
	case _IOC_WRITE:
	case (_IOC_WRITE | _IOC_READ):
		if (_IOC_SIZE(cmd) <= sizeof(sbuf)) {
			parg = sbuf;
		} else {
			/* too big to allocate from stack */
			mbuf = kmalloc(_IOC_SIZE(cmd), GFP_KERNEL);
			if (!mbuf)
				return -ENOMEM;
			parg = mbuf;
		}

		err = -EFAULT;
		if (copy_from_user(parg, (void __user *)arg, _IOC_SIZE(cmd)))
			goto out;
		break;
	}

	/* call driver */
	err = func(file, cmd, parg);
	if (err == -ENOIOCTLCMD)
		err = -ENOTTY;

	if (err < 0)
		goto out;

	/*  Copy results into user buffer  */
	switch (_IOC_DIR(cmd)) {
	case _IOC_READ:
	case (_IOC_WRITE | _IOC_READ):
		if (copy_to_user((void __user *)arg, parg, _IOC_SIZE(cmd)))
			err = -EFAULT;
		break;
	}

out:
	kfree(mbuf);
	return err;
}

int dmx_key_open(struct inode *inode, struct file *file)
{
	mutex_lock(&mutex);

	file->private_data = vmalloc(KTE_MAX);
	if (file->private_data)
		memset(file->private_data, 0xff, KTE_MAX);
	pr_dbg("%s line:%d\n", __func__, __LINE__);
	mutex_unlock(&mutex);

	return 0;
}

int dmx_key_close(struct inode *inode, struct file *file)
{
	int i = 0;
	char index = 0;

	mutex_lock(&mutex);
	if (file->private_data) {
		for (i = 0; i < KTE_MAX; i++) {
			index = *(char *)(file->private_data + i);
			if (index != 0xff) {
				kt_free(index);
				*(char *)(file->private_data + i) = 0xff;
			}
		}
	}
	mutex_unlock(&mutex);
	pr_dbg("%s line:%d\n", __func__, __LINE__);
	return 0;
}

int dmx_key_do_ioctl(struct file *file, unsigned int cmd, void *parg)
{
	int ret = -EINVAL;

	mutex_lock(&mutex);
	switch (cmd) {
	case KEY_MALLOC_SLOT:{
			u32 kte;
			struct key_config *d = parg;

			if (kt_alloc(&kte, d) == 0) {
				d->key_index = kte;
				ret = 0;
				if (file->private_data)
					*(char *)(file->private_data + kte) =
					    kte;
			}
			break;
		}
	case KEY_FREE_SLOT:{
			u32 kte = (unsigned long)parg;

			if (kt_free(kte) == 0) {
				ret = 0;
				if (file->private_data)
					*(char *)(file->private_data + kte) =
					    0xff;
			}
			break;
		}
	case KEY_SET:{
			struct key_descr *d = parg;

			if (kt_set(d->key_index, d->key, d->key_len) == 0)
				ret = 0;
			break;
		}
	default:
		break;
	}

	mutex_unlock(&mutex);

	return 0;
}

long dmx_key_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return usercopy(file, cmd, arg, dmx_key_do_ioctl);
}

static const struct file_operations device_fops = {
	.owner = THIS_MODULE,
	.open = dmx_key_open,
	.release = dmx_key_close,
	.read = NULL,
	.write = NULL,
	.poll = NULL,
	.unlocked_ioctl = dmx_key_ioctl,
};

int dmx_key_init(void)
{
	struct device *clsdev;

	pr_dbg("%s enter\n", __func__);
	major_key = register_chrdev(0, "key", &device_fops);
	if (major_key < 0) {
		dprint("error:can not register key device\n");
		return -1;
	}

	if (!dmx_key_class) {
		dmx_key_class = class_create(THIS_MODULE, "key");
		if (IS_ERR(dmx_key_class)) {
			dprint("class create dmx_key_class fail\n");
			return -1;
		}
	}
	clsdev = device_create(dmx_key_class, NULL,
			       MKDEV(major_key, 0), NULL, "key");
	if (IS_ERR(clsdev)) {
		dprint("device_create key fail\n");
		return PTR_ERR(clsdev);
	}
	mutex_init(&mutex);
	kt_init();
	return 0;
}

void dmx_key_exit(void)
{
	pr_dbg("%s enter\n", __func__);

	device_destroy(dmx_key_class, MKDEV(major_key, 0));
	unregister_chrdev(major_key, "key");

	class_destroy(dmx_key_class);
	dmx_key_class = NULL;
}

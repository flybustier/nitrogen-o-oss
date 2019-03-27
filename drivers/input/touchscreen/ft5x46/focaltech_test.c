
/*
 * FocalTech TouchScreen driver.
 * Copyright (c) 2010-2016, FocalTech Systems, Ltd., all rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/************************************************************************
* File Name: focaltech_test.c
* Author:	  Software Department, FocalTech
* Created: 2016-03-24
* Modify:
* Abstract: create char device and proc node for  the comm between APK and TP
************************************************************************/

/*******************************************************************************
* Included header files
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include "focaltech_test.h"
#include "focaltech_test_ft5x46.h"

/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define FOCALTECH_TEST_INFO  "focaltech_test.c:V1.1.0 2016-05-19"
#define DEVIDE_MODE_ADDR	0x00
static struct proc_dir_entry *focal_selftest_proc;
static struct proc_dir_entry *focal_datadump_proc;
/*******************************************************************************
* functions body
*******************************************************************************/
int focal_test_result = RESULT_INVALID;

static int focal_i2c_read_test(unsigned char *writebuf, int writelen,
			       unsigned char *readbuf, int readlen)
{
	int iret = -1;

	iret = focal_i2c_read(focal_i2c_client, writebuf,
			      writelen, readbuf, readlen);
	return iret;
}

static int focal_i2c_write_test(unsigned char *writebuf, int writelen)
{
	int iret = -1;

	iret = focal_i2c_write(focal_i2c_client, writebuf, writelen);
	return iret;
}

void focal_msleep(int ms)
{
	msleep(ms);
}

void SysDelay(int ms)
{
	msleep(ms);
}

int focal_abs(int value)
{
	if (value < 0)
		value = 0 - value;

	return value;
}

void *focal_malloc(size_t size)
{
	if (FOCAL_MALLOC_TYPE == kmalloc_mode)
		return kmalloc(size, GFP_ATOMIC);

	if (FOCAL_MALLOC_TYPE == vmalloc_mode)
		return vmalloc(size);

	return NULL;
}

void focal_free(void *p)
{
	if (FOCAL_MALLOC_TYPE == kmalloc_mode)
		kfree(p);

	if (FOCAL_MALLOC_TYPE == vmalloc_mode)
		vfree(p);
}

int ReadReg(unsigned char RegAddr, unsigned char *RegData)
{
	int iRet;
	iRet = focal_i2c_read_test(&RegAddr, 1, RegData, 1);

	if (iRet >= 0)
		return ERROR_CODE_OK;
	else
		return ERROR_CODE_COMM_ERROR;
}

int WriteReg(unsigned char RegAddr, unsigned char RegData)
{
	int iRet;
	unsigned char cmd[2] = {0};
	cmd[0] = RegAddr;
	cmd[1] = RegData;
	iRet = focal_i2c_write_test(cmd, 2);

	if (iRet >= 0)
		return ERROR_CODE_OK;
	else
		return ERROR_CODE_COMM_ERROR;
}

unsigned char Comm_Base_IIC_IO(unsigned char *pWriteBuffer, int iBytesToWrite,
			       unsigned char *pReadBuffer, int iBytesToRead)
{
	int iRet;

	iRet = focal_i2c_read_test(pWriteBuffer, iBytesToWrite,
				   pReadBuffer, iBytesToRead);
	if (iRet >= 0)
		return ERROR_CODE_OK;
	else
		return ERROR_CODE_COMM_ERROR;
}

unsigned char EnterWork(void)
{
	unsigned char RunState = 0;
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;

	FOCAL_TEST_DBG("");
	ReCode = ReadReg(DEVIDE_MODE_ADDR, &RunState);

	if (ReCode == ERROR_CODE_OK) {
		if (((RunState >> 4) & 0x07) == 0x00)
			ReCode = ERROR_CODE_OK;
		else {
			ReCode = WriteReg(DEVIDE_MODE_ADDR, 0);

			if (ReCode == ERROR_CODE_OK) {
				ReCode = ReadReg(DEVIDE_MODE_ADDR, &RunState);

				if (ReCode == ERROR_CODE_OK) {
					if (((RunState >> 4) & 0x07) == 0x00)
						ReCode = ERROR_CODE_OK;
					else
						ReCode = ERROR_CODE_COMM_ERROR;
				}
			} else
				pr_err("%s,error\n", __func__);
		}
	}
	return ReCode;
}

unsigned char EnterFactory(void)
{
	unsigned char RunState = 0;
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;
	FOCAL_TEST_DBG("");
	ReCode = ReadReg(DEVIDE_MODE_ADDR, &RunState);

	if (ReCode == ERROR_CODE_OK) {
		if (((RunState >> 4) & 0x07) == 0x04)
			ReCode = ERROR_CODE_OK;
		else {
			ReCode = WriteReg(DEVIDE_MODE_ADDR, 0x40);

			if (ReCode == ERROR_CODE_OK) {
				ReCode = ReadReg(DEVIDE_MODE_ADDR, &RunState);

				if (ReCode == ERROR_CODE_OK) {
					if (((RunState >> 4) & 0x07) == 0x04)
						ReCode = ERROR_CODE_OK;
					else
						ReCode = ERROR_CODE_COMM_ERROR;
				}
			}
		}
	} else
		FOCAL_TEST_ERR("EnterFactory read DEVIDE_MODE_ADDR error 1.");

	FOCAL_TEST_DBG(" END");
	return ReCode;
}

static int focal_i2c_test(struct i2c_client *client)
{
	int retry = 5;
	int ReCode = ERROR_CODE_OK;
	unsigned char chip_id = 0;

	if (client == NULL)
		return RESULT_INVALID;

	focal_test_result = RESULT_INVALID;

	while (retry--) {
		ReCode = ReadReg(REG_CHIP_ID, &chip_id);

		if (ReCode == ERROR_CODE_OK)
			return RESULT_PASS;

		dev_err(&client->dev, "i2c test failed time %d.\n", retry);
		msleep(20);
	}

	if (ReCode != ERROR_CODE_OK)
		return RESULT_NG;

	return RESULT_INVALID;
}

static void focal_open_test(struct i2c_client *client)
{
	int retry = 3;

	focal_test_result = RESULT_INVALID;
	while (retry--) {
		focal_test_result = FT5X46_TestItem_RawDataTest(focal_i2c_client);
		if (focal_test_result == RESULT_PASS)
			return;
		dev_err(&client->dev, "open test failed time %d.\n", retry);
		msleep(20);
	}
}

static void focal_short_test(struct i2c_client *client)
{
	int retry = 3;

	focal_test_result = RESULT_INVALID;
	while (retry--) {
		focal_test_result = FT5X46_TestItem_WeakShortTest(focal_i2c_client);
		if (focal_test_result == RESULT_PASS)
			return;
		dev_err(&client->dev, "short test failed time %d.\n", retry);
		msleep(20);
	}
}

static ssize_t focal_tp_selftest_read(struct file *file, char __user *buf,
			       size_t count, loff_t *pos)
{
	char tmp[5];
	int cnt;

	if (*pos != 0)
		return 0;

	cnt = snprintf(tmp, sizeof(focal_test_result), "%d\n", focal_test_result);
	if (copy_to_user(buf, tmp, strlen(tmp)))
		return -EFAULT;
	*pos += cnt;
	return cnt;
}

static ssize_t focal_tp_selftest_write(struct file *file, const char __user *buf,
				size_t count, loff_t *pos)
{
	char tmp[6];
	int retval;

	if (!focal_i2c_client || count > sizeof(tmp)) {
		retval = -EINVAL;
		focal_test_result = RESULT_INVALID;
		goto out;
	}
	if (copy_from_user(tmp, buf, count)) {
		retval = -EFAULT;
		focal_test_result = RESULT_INVALID;
		goto out;
	}

	disable_irq(focal_i2c_client->irq);

	if (!strncmp(tmp, "short", 5))
		focal_short_test(focal_i2c_client);
	else if (!strncmp(tmp, "open", 4))
		focal_open_test(focal_i2c_client);
	else if (!strncmp(tmp, "i2c", 3))
		focal_test_result = focal_i2c_test(focal_i2c_client);
	retval = focal_test_result;
	EnterWork();
	enable_irq(focal_i2c_client->irq);
out:
	if (retval >= 0)
		retval = count;
	return retval;
}

static ssize_t focal_tp_datadump_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	int ret = 0, cnt = 0;
	char *tmp = NULL;

	if (*pos != 0)
		return 0;
	tmp = kzalloc(PAGE_SIZE * 2, GFP_KERNEL);
	if (tmp == NULL)
		return 0;
	disable_irq(focal_i2c_client->irq);
	EnterFactory();
	SysDelay(100);
	cnt = FT5X46_TestItem_GetRawData(tmp, 0);
	if (cnt == 0)
		goto out;
	ret = FT5X46_TestItem_GetRawData(tmp + cnt, 1);
	if (ret == 0)
		goto out;
	if (copy_to_user(buf, tmp, cnt + ret))
		ret = -EFAULT;
out:
	if (tmp) {
		kfree(tmp);
		tmp = NULL;
	}
	if (ret <= 0)
		return ret;
	*pos += (cnt + ret);
	EnterWork();
	enable_irq(focal_i2c_client->irq);
	return cnt + ret;
}

static const struct file_operations tp_selftest_ops = {
	.read		= focal_tp_selftest_read,
	.write		= focal_tp_selftest_write,
};

static const struct file_operations focal_tp_datadump_ops = {
	.read		= focal_tp_datadump_read,
};

int focal_test_module_init(struct i2c_client *client)
{
	FOCAL_TEST_DBG("[focal] %s ",  FOCALTECH_TEST_INFO);
	focal_selftest_proc = proc_create("tp_selftest", S_IRWXU, NULL, &tp_selftest_ops);
	focal_datadump_proc = proc_create("tp_data_dump", 0, NULL, &focal_tp_datadump_ops);
	return 0;
}

int focal_test_module_exit(struct i2c_client *client)
{
	FOCAL_TEST_DBG("");

	if (focal_selftest_proc)
		remove_proc_entry("tp_selftest", NULL);
	if (focal_datadump_proc)
		remove_proc_entry("tp_data_dump", NULL);
	return 0;
}

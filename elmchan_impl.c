/*----------------------------------------------------------------------------/
/  FatBoy - Simple FAT file system tool                                       /
/-----------------------------------------------------------------------------/
/
/ Copyright (C) 2017, John Kelley
/ All right reserved.
/
/ FatBoy is open source software. Redistribution and use of FatBoy in source
/ and binary forms, with or without modification, are permitted provided
/ that the following condition is met:
/
/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition, and the following disclaimer.
/
/ THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
/ ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
/ WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
/ DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
/ ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
/ (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
/ LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
/ ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
/ (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
/ SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "elmchan_impl.h"
#include "elmchan/src/diskio.h"

static char image_path[4096];
static FILE *image = NULL;
static uint32_t image_size = 0;

static const char *FR_RESULT_Strings[] = {
	"FR_OK",                  /* (0) Succeeded */
	"FR_DISK_ERR",            /* (1) A hard error occurred in the low level disk I/O layer */
	"FR_INT_ERR",             /* (2) Assertion failed */
	"FR_NOT_READY",           /* (3) The physical drive cannot work */
	"FR_NO_FILE",             /* (4) Could not find the file */
	"FR_NO_PATH",             /* (5) Could not find the path */
	"FR_INVALID_NAME",        /* (6) The path name format is invalid */
	"FR_DENIED",              /* (7) Access denied due to prohibited access or directory full */
	"FR_EXIST",               /* (8) Access denied due to prohibited access */
	"FR_INVALID_OBJECT",      /* (9) The file/directory object is invalid */
	"FR_WRITE_PROTECTED",     /* (10) The physical drive is write protected */
	"FR_INVALID_DRIVE",       /* (11) The logical drive number is invalid */
	"FR_NOT_ENABLED",         /* (12) The volume has no work area */
	"FR_NO_FILESYSTEM",       /* (13) There is no valid FAT volume */
	"FR_MKFS_ABORTED",        /* (14) The f_mkfs() aborted due to any problem */
	"FR_TIMEOUT",             /* (15) Could not get a grant to access the volume within defined period */
	"FR_LOCKED",              /* (16) The operation is rejected according to the file sharing policy */
	"FR_NOT_ENOUGH_CORE",     /* (17) LFN working buffer could not be allocated */
	"FR_TOO_MANY_OPEN_FILES", /* (18) Number of open files > _FS_LOCK */
	"FR_INVALID_PARAMETER"    /* (19) Given parameter is invalid */
};

const char*
fr_res_to_str(uint32_t fr_res) {
	return FR_RESULT_Strings[fr_res];
}

int32_t
fatboy_set_image(const char *path) {
	image = fopen(path, "r+b");
	if (!image) {
		printf("ERROR: could not open image '%s'\n", path);
		return -1;
	}

	strncpy(image_path, path, sizeof(image_path));
	image_path[sizeof(image_path)-1] = '\0';

	fseek(image, 0, SEEK_END);
	image_size = ftell(image);

	if (image_size % FATBOY_SECTOR_SIZE != 0) {
		printf("ERROR: %d is not a multiple of 512 bytes\n", image_size);
		fclose(image);
		memset(image_path, '\0', sizeof(image_path));
		image_size = 0;
		return -2;
	}
	return 0;
}

DWORD
get_fattime(void) {
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	uint32_t fattime = 0;

	// tm_year is based from 1900, elmchan expects 1980 base
	fattime |= ((tm.tm_year - 80) & 0x7F) << 25;
	fattime |= ((tm.tm_mon + 1) & 0x0F) << 21;
	fattime |= (tm.tm_mday & 0x1f) << 16;
	fattime |= (tm.tm_hour & 0x1f) << 11;
	fattime |= (tm.tm_min  & 0x3f) << 5;
	fattime |= (tm.tm_sec  & 0x1f);

	return fattime;
}

DSTATUS
RAM_disk_initialize() {
	if (!image) {
		printf("ERR\n");
		return STA_NOINIT;
	}

	return 0;
}

DSTATUS
RAM_disk_status() {
	return RAM_disk_initialize();
}

DRESULT
RAM_disk_read(BYTE* buff, DWORD sector, UINT count) {
	if (!image) {
		return RES_NOTRDY;
	}

	fseek(image, FATBOY_SECTOR_SIZE * sector, SEEK_SET);
	size_t sectors_read = fread(buff, FATBOY_SECTOR_SIZE, count, image);
	if (sectors_read != count) {
		printf("Short read of %d sectors instead of %d\n", sectors_read, count);
		return RES_ERROR;
	}
	return RES_OK;
}

DRESULT
RAM_disk_write(const BYTE* buff, DWORD sector, UINT count) {
	if (!image) {
		return RES_NOTRDY;
	}

	fseek(image, FATBOY_SECTOR_SIZE * sector, SEEK_SET);
	size_t sectors_wrote = fwrite(buff, FATBOY_SECTOR_SIZE, count, image);
	if (sectors_wrote != count) {
		printf("Short write of %d sectors instead of %d\n", sectors_wrote, count);
		return RES_ERROR;
	}
	//printf("wrote %d sectors starting at sector %d\n", sectors_wrote, sector);
	return RES_OK;
}

DRESULT
RAM_disk_ioctl(BYTE cmd, void* buff) {
	union ptrs {
		void* ptr_void;
		WORD* ptr_word;
		DWORD* ptr_dword;
	} ptrs;

	if (!image) {
		printf("NO IMAGE!\n");
		return RES_NOTRDY;
	}

	ptrs.ptr_void = buff;

	switch (cmd) {
		case CTRL_SYNC:
			fflush(image);
			break;
		case GET_SECTOR_COUNT:
			*ptrs.ptr_dword = image_size / FATBOY_SECTOR_SIZE;
			break;
		case GET_SECTOR_SIZE:
		case GET_BLOCK_SIZE:
			*ptrs.ptr_word = FATBOY_SECTOR_SIZE;
			break;
		default:
			return RES_PARERR;
	};
	return RES_OK;
}

// Unused
DSTATUS
MMC_disk_status(BYTE pdrv) {
	printf("%s called!!!!\n", __FUNCTION__);
	return STA_NOINIT;
}

DSTATUS
MMC_disk_initialize(BYTE pdrv) {
	printf("%s called!!!!\n", __FUNCTION__);
	return STA_NOINIT;
}

DRESULT
MMC_disk_read(BYTE* buff, DWORD sector, UINT count) {
	return RES_NOTRDY;
}

DRESULT
MMC_disk_write(const BYTE* buff, DWORD sector, UINT count) {
	return RES_NOTRDY;
}

DRESULT
MMC_disk_ioctl(BYTE cmd, void* buff) {
	return RES_NOTRDY;
}

DSTATUS
USB_disk_status(BYTE pdrv) {
	printf("%s called!!!!\n", __FUNCTION__);
	return STA_NOINIT;
}

DSTATUS
USB_disk_initialize(BYTE pdrv) {
	printf("%s called!!!!\n", __FUNCTION__);
	return STA_NOINIT;
}

DRESULT
USB_disk_read(BYTE* buff, DWORD sector, UINT count) {
	return RES_NOTRDY;
}

DRESULT
USB_disk_write(const BYTE* buff, DWORD sector, UINT count) {
	return RES_NOTRDY;
}

DRESULT
USB_disk_ioctl(BYTE cmd, void* buff) {
	return RES_NOTRDY;
}


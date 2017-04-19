#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "elmchan/src/diskio.h"

// support the smallest sector size for maximal compatibility with underlying images
#define FATBOY_SECTOR_SIZE 512
static char image_path[4096];
static FILE *image = NULL;
static uint32_t image_size = 0;

int32_t
fatboy_set_image(const char *path) {
	image = fopen(path, "r+b");
	if (!image) {
		printf("ERROR: could not open image '%s'\n", path);
		return -1;
	}

	printf("opened '%s'\n", path);	
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
			fsync(image);
			break;
		case GET_SECTOR_COUNT:
			printf("returning sector count %d\n", image_size / FATBOY_SECTOR_SIZE);
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
	return STA_NOINIT;
}

DSTATUS
MMC_disk_initialize(BYTE pdrv) {
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
	return STA_NOINIT;
}

DSTATUS
USB_disk_initialize(BYTE pdrv) {
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


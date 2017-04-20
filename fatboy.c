#include <ctype.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "elmchan_impl.h"
#include "elmchan/src/diskio.h"
#include "elmchan/src/ff.h"

int main(int argc, const char *argv[]) {
	const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	const char* fat_types[] = {"None", "FAT-12", "FAT-16", "FAT-32", "ExFAT"};
	const char *image_path = argv[1];
	const char *action = argv[2];
	FATFS fs;
	int32_t ret;
	int exit_code = 0;

	if (argc < 3) {
		printf("Usage: %s <image> <action> <parameters>\n", basename((char *)argv[0]));
		printf("Actions:\n");
		printf("\tls <path> - print a file listing for an optional path\n");
		printf("\trm <path> - remove a file from the image\n");
		printf("\tadd <host_file> (<image_path>) - add a file from the host to / or the specified image path\n");
		printf("\textract <image_path> (<host_file>) - extract a file from the image to the specified file or current directory\n");
		printf("\tinfo - print information about the image\n");
		printf("\tmkdir <image_path> - make a directory\n");
		printf("\tmkfs <fat, fat32, exfat, any> (<power of 2 allocation unit>) - make a new filesystem with an optional allocation unit size\n");
		printf("\tsetlabel <label> - set FS label\n");
		return -1;
	}

	ret = fatboy_set_image(image_path);
	if (ret != 0) {
		printf("Error %d opening FAT image '%s'\n", ret, image_path);
		return -1;
	}

	// MKFS needs to hapen before we try and mount the partition
	if (strcmp(action, "mkfs") == 0) {
		FRESULT res;
		DWORD alloc_unit = 0;
		uint8_t *work;
		uint32_t work_len = 64 *1024;
		char fat_lower[8] = {0};
		char *fat_args[] = {"fat", "fat32", "exfat", "any"};
		BYTE fat_type[] = {FM_FAT, FM_FAT32, FM_EXFAT, FM_ANY};
		uint8_t fat_type_index = sizeof(fat_type) / sizeof(BYTE);;
		BYTE fs_type = FM_ANY;

		if (argc > 3) {
			for (int i = 0; i < sizeof(fat_lower) - 1; ++i) {
				int ret = tolower((unsigned char)argv[3][i]);
				fat_lower[i] = ret & 0xFF;
			}

			for (int i = 0; i < sizeof(fat_type) / sizeof(BYTE); ++i) {
				if (strcmp(fat_lower, fat_args[i]) == 0) {
					fs_type = fat_type[i];
					fat_type_index = i;
					break;
				}
			}

			if (fat_type_index == 4) {
				printf("Invalid fs type '%s'\n", argv[3]);
				exit_code = -1;
				goto exit;
			}
		} else {
			fat_type_index = 3;
		}

		if (argc > 4) {
			alloc_unit = atoi(argv[4]);
			printf("Creating a FS with type %s and allocation unit of %lu bytes\n", fat_args[fat_type_index], alloc_unit);
		} else {
			printf("Creating a FS with type %s and default allocation unit\n", fat_args[fat_type_index]);
		}

		work = malloc(work_len);
		if (!work) {
			printf("Failed to allocate work buffer for mkfs\n");
			exit_code = -1;
			goto exit;
		}

		res = f_mkfs("", fs_type, alloc_unit, work, work_len);

		free(work);
		work = NULL;

		if (res != FR_OK) {
			printf("Filesystem creation failed with %d\n", res);
			exit_code = -1;
			goto exit;
		}
		// set action to info so we print out information about our new FS
		action = "info";
	}

	// mount the partition for use by other commands
	ret = f_mount(&fs, "", 1);
	if (ret != FR_OK) {
		printf("Error 0x%x mounting volume\n", ret);
		return -1;
	}

	if (strcmp(action, "ls") == 0) {
		char path[4096];
		if (argv[3]) {
	       		strncpy(path, argv[3], sizeof path);
		}
		FRESULT res;
		DIR dir;
		UINT i;
		static FILINFO fno;

		res = f_opendir(&dir, path);
		if (res == FR_OK) {
			for (;;) {
				res = f_readdir(&dir, &fno);
				if (res != FR_OK || fno.fname[0] == 0) {
					break;  // Break on error or end of dir
				}
				printf("%c %10llu %s %d %d %02d:%02d %s/%s\n", fno.fattrib & AM_DIR ? 'd' : '-', fno.fsize, months[(fno.fdate >> 5) & 0xF], fno.fdate&0xF, (fno.fdate >> 9) + 1980, (fno.ftime>>11)&0xF, (fno.ftime>>5)&0x1F , path, fno.fname);
			}
			f_closedir(&dir);
		} else {
			printf("Couldn't open '%s' to list\n", path);
		}
	} else if (strcmp(action, "rm") == 0) {
		const char *path = argv[3];
		FRESULT res;

		if (!path) {
			printf("Error: path to rm not specfied\n");
			exit_code = -1;
			goto exit;
		}
		res = f_unlink(path);
		if (res == FR_OK) {
			printf("Removed '%s'\n", path);
		} else {
			printf("Error %d unlinking '%s'\n", res, path);
			exit_code = -1;
		}
	} else if (strcmp(action, "add") == 0) {
		const char *host_file = argv[3];
		const char *fat_file = argv[4];
		FRESULT res;
		FIL fp;
		FILE *fin;
		BYTE buffer[4096];
		uint32_t bytes_read, bytes_wrote;

		if (!host_file) {
			printf("File to add not specified\n");
			exit_code = -1;
			goto exit;
		}
		if (!fat_file) {
			printf("Placing file in root directory\n");
			fat_file = basename((char *)host_file);
		}
		printf("Adding '%s' to '%s'\n", host_file, fat_file);

		res = f_open(&fp, fat_file, FA_WRITE|FA_CREATE_ALWAYS);
		if (res != FR_OK) {
			printf("Open failed with %d\n", res);
			exit_code = -1;
			goto exit;
			return -1;
		}

		fin = fopen(host_file, "rb");
		if (!fin) {
			printf("couldn't open '%s' for writing\n", host_file);
			exit_code = -1;
			goto exit;
			return -1;
		}

		for (;;) {
			bytes_read = fread(buffer, 1, sizeof buffer, fin);
			if (bytes_read == 0 && ferror(fin) != 0) {
				printf("ferror: %d\n", ferror(fin));
				exit_code = -1;
				break;
			}
			res = f_write(&fp, buffer, bytes_read, &bytes_wrote);
			if (res != RES_OK || bytes_wrote < bytes_read) {
				printf("Error: could only write %d bytes instead of %d\n", bytes_wrote, bytes_read);
				exit_code = -1;
				break;
			}
			if (feof(fin) != 0) {
				break;
			}
		}
		fclose(fin);
		f_close(&fp);

	} else if (strcmp(action, "extract") == 0) {
		const char *fat_file = argv[3];
		const char *host_file = argv[4];
		FRESULT res;
		FIL fp;
		FILE *out;
		BYTE buffer[4096];
		uint32_t bytes_read, bytes_wrote;

		if (!fat_file) {
			printf("File to extract not specified\n");
			exit_code = -1;
			goto exit;
		}
		if (!host_file) {
			host_file = basename((char *)fat_file);
		}
		printf("Extracting '%s' to '%s'\n", fat_file, host_file);

		res = f_open(&fp, fat_file, FA_READ);
		if (res != FR_OK) {
			printf("Open failed with %d\n", res);
			exit_code = -1;
			goto exit;
			return -1;
		}

		out = fopen(host_file, "wb");
		if (!out) {
			printf("couldn't open '%s' for writing\n", host_file);
			exit_code = -1;
			goto exit;
			return -1;
		}

		for (;;) {
			res = f_read(&fp, buffer, sizeof buffer, &bytes_read);
			if (res != RES_OK || bytes_read == 0) {
				exit_code = -1;
				break;
			}
			bytes_wrote = fwrite(buffer, 1, bytes_read, out);
			if (bytes_wrote< bytes_read) {
				printf("Error: could only write %d bytes instead of %d\n", bytes_wrote, bytes_read);
				exit_code = -1;
				break;
			}
		}
		fclose(out);
		f_close(&fp);
	} else if (strcmp(action, "info") == 0) {
		FRESULT res;
		TCHAR label[256];
		DWORD vsn;
		res = f_getlabel("", label, &vsn);
		if (res != FR_OK) {
			printf("Error getting label: %d\n", res);
		} else {
			printf("Label: '%s'\nSerial: 0x%08lX\n", label, vsn);
		}

		DWORD clusters;
		FATFS *fatfs;
		res = f_getfree("", &clusters, &fatfs);
		if (res != FR_OK) {
			printf("Error getting free space: %d\n", res);
		} else {
			printf("FS type: %s\n", fat_types[fatfs->fs_type]);
			printf("Free space: %lu KiB\n", clusters * fatfs->csize * FATBOY_SECTOR_SIZE / 1024);
			printf("Capacity:   %lu KiB\n", (fatfs->n_fatent -2) * fatfs->csize * FATBOY_SECTOR_SIZE / 1024);
		}
	} else if (strcmp(action, "mkdir") == 0) {
		FRESULT res;
		const char *path = argv[3];

		if (!path) {
			printf("Error: path to create was not specified\n");
			exit_code = -1;
			goto exit;
		}

		res = f_mkdir(path);
		if (res == FR_OK) {
			printf("Created '%s'\n", path);
		} else {
			printf("Error %d creating directory '%s'\n", res, path);
			exit_code = -1;
		}
	} else if (strcmp(action, "setlabel") == 0) {
		FRESULT res;
		const char *label = argv[3];

		if (argc < 4 || !label) {
			printf("Error: label was not specified\n");
			exit_code = -1;
			goto exit;
		}

		res = f_setlabel(label);
		if (res == FR_OK) {
			printf("Label set to '%s'\n", label);
		} else {
			printf("Error %d setting label '%s'\n", res, label);
			exit_code = -1;
		}
	} else {
		printf("Invalid action '%s'\n", action);
	}

exit:
	f_mount(NULL, "", 0);
	return exit_code;
}

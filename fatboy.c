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
#include <ctype.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "elmchan_impl.h"
#include "elmchan/src/diskio.h"
#include "elmchan/src/ff.h"
#include "util.h"

struct FatType {
	char *name;
	BYTE id;
};

static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static const char* fatfs_names[] = {"None", "FAT-12", "FAT-16", "FAT-32", "ExFAT"};

int main(int argc, const char *argv[]) {
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
		printf("\textractdir <image_dir> (<host_dir>) - extract a directory from the image to the specified or current directory. Non-recursive.\n");
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
		const char *fat_arg = argv[3];
		const char *alloc_arg = argv[4];
		struct FatType *selected = NULL;
		struct FatType fs_types[] = {
			{"any", FM_ANY} , {"fat", FM_FAT},
			{"fat32", FM_FAT32}, {"exfat", FM_EXFAT},
		};

		if (argc > 3) {
			for (int i = 0; i < sizeof(fs_types) / sizeof(struct FatType); ++i) {
				if (strcmp(fat_arg, fs_types[i].name) == 0) {
					selected = &fs_types[i];
					break;
				}
			}

			if (selected == NULL) {
				printf("Invalid fs type '%s'\n", fat_arg);
				exit_code = -1;
				goto exit;
			}
		} else {
			// default to autodetect
			selected = &fs_types[0];
		}

		if (argc > 4) {
			alloc_unit = (DWORD)atoi(alloc_arg);
			printf("Creating FS of type %s and allocation unit of %lu bytes\n", selected->name, alloc_unit);
		} else {
			printf("Creating FS of type %s and default allocation unit\n", selected->name);
		}

		work = malloc(work_len);
		if (!work) {
			printf("Failed to allocate work buffer for mkfs\n");
			exit_code = -1;
			goto exit;
		}

		res = f_mkfs("", selected->id, alloc_unit, work, work_len);

		free(work);
		work = NULL;

		if (res != FR_OK) {
			printf("Filesystem creation failed: %s\n", fr_res_to_str(res));
			exit_code = -1;
			goto exit;
		}
		// set action to info so we print out information about our new FS
		action = "info";
	}

	// mount the partition for use by other commands
	ret = f_mount(&fs, "", 1);
	if (ret != FR_OK) {
		printf("Error mounting volume: %s\n", fr_res_to_str(ret));
		return -1;
	}

	if (strcmp(action, "ls") == 0) {
		char path[4096] = ".";
		if (argv[3]) {
	       		strncpy(path, argv[3], sizeof path);
		}
		FRESULT res;
		DIR dir;
		static FILINFO fno;

		res = f_opendir(&dir, path);
		if (res != FR_OK) {
			printf("'%s' not found (%s)\n", path, fr_res_to_str(res));
			exit_code = -1;
			goto exit;
		}
		for (;;) {
			res = f_readdir(&dir, &fno);
			  // Break on error or end of dir
			if (res != FR_OK || fno.fname[0] == 0) {
				break;
			}
			printf("%c %10llu %s %d %d %02d:%02d %s/%s\n", (fno.fattrib & AM_DIR) ? 'd' : '-',
					fno.fsize, months[(fno.fdate >> 5) & 0xF], fno.fdate & 0xF,
					(fno.fdate >> 9) + 1980, (fno.ftime>>11) & 0xF,
					(fno.ftime>>5) & 0x1F, path, fno.fname);
		}
		f_closedir(&dir);

	} else if (strcmp(action, "rm") == 0) {
		const char *path = argv[3];
		FRESULT res;

		if (!path) {
			printf("Error: path argument to rm not specfied\n");
			exit_code = -1;
			goto exit;
		}
		res = f_unlink(path);
		if (res == FR_OK) {
			printf("Removed '%s'\n", path);
		} else {
			printf("Error removing '%s': %s\n", path, fr_res_to_str(res));
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
			printf("Open failed: %s\n", fr_res_to_str(res));
			exit_code = -1;
			goto exit;
			return -1;
		}

		//TODO: detect adding to a directory name and automatically fix

		fin = fopen(host_file, "rb");
		if (!fin) {
			printf("Error: couldn't open '%s' for reading\n", host_file);
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
			printf("Error: Open failed: %s\n", fr_res_to_str(res));
			exit_code = -1;
			goto exit;
		}

		out = fopen(host_file, "wb");
		if (!out) {
			printf("Error: couldn't open '%s' for writing\n", host_file);
			exit_code = -1;
			goto exit;
		}

		exit_code = write_file(&fp, out);
		if (exit_code != 0) {
			goto exit;
		}

		fclose(out);
		f_close(&fp);

	} else if (strcmp(action, "info") == 0) {
		FRESULT res;
		TCHAR label[256];
		DWORD vsn;
		res = f_getlabel("", label, &vsn);
		if (res != FR_OK) {
			printf("Error getting label: %s\n", fr_res_to_str(res));
		} else {
			printf("Label: '%s'\nSerial: 0x%08lX\n", label, vsn);
		}

		DWORD clusters;
		FATFS *fatfs;
		res = f_getfree("", &clusters, &fatfs);
		if (res != FR_OK) {
			printf("Error getting free space: %s\n", fr_res_to_str(res));
		} else {
			printf("FS type: %s\n", fatfs_names[fatfs->fs_type]);
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
			printf("Error creating directory '%s': %s\n", path, fr_res_to_str(res));
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
			printf("Error setting label to '%s': %s\n", label, fr_res_to_str(res));
			exit_code = -1;
		}

	}  else if(strcmp(action, "extractdir") == 0) {
		char img_path[3072];
		char host_path[3072];

		strncpy(img_path, argv[3], sizeof img_path);

		if (argv[4]) {
			strncpy(host_path, argv[4], sizeof host_path);
		} else {
			strncpy(host_path, ".", sizeof host_path);
		}

		static FILINFO fno;
		FRESULT res;
		DIR dir;
		FILE *out;
		FIL fp;

		res = f_opendir(&dir, img_path);
		if (res == FR_OK) {
			char img_fname[4096];
			char host_fname[4096];
			for (;;) {
				res = f_readdir(&dir, &fno);

				if (res != FR_OK || fno.fname[0] == 0) {
					break;  // Break on error or end of dir
				}

				if (fno.fattrib & AM_DIR) {
					continue; // Ignore directories
				}

				strncpy(img_fname, img_path, sizeof img_path);
				strncat(img_fname, "/", 1);
				strncat(img_fname, fno.fname, sizeof img_fname - sizeof img_path - 1);

				strncpy(host_fname, host_path, sizeof host_fname);
				strncat(host_fname, "/", 1);
				strncat(host_fname, fno.fname, sizeof host_fname - sizeof host_path - 1);

				printf("Extracting %s to %s\n", img_fname, host_fname);

				res = f_open(&fp, img_fname, FA_READ);
				if (res != FR_OK) {
					printf("Open failed with %d\n", res);
					exit_code = -1;
					goto exit;
				}

				out = fopen(host_fname, "wb");
				if (!out) {
					printf("couldn't open '%s' for writing\n", host_fname);
					exit_code = -1;
					goto exit;
				}

				exit_code = write_file(&fp, out);
				if (exit_code != 0) {
					goto exit;
				}
			}
			f_closedir(&dir);
		} else {
			printf("Couldn't open '%s' to list\n", img_path);
		}
	} else {
		printf("Invalid action '%s'\n", action);
	}

exit:
	f_mount(NULL, "", 0);
	return exit_code;
}

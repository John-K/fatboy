#include <stdlib.h>
#include <string.h>

#include "elmchan_impl.h"
#include "elmchan/src/diskio.h"
#include "elmchan/src/ff.h"

#include "util.h"

int write_file(FIL *image_fp, FILE *host_file)
{
	char buffer[4096];
	int exit_code = 0;
	int res;

	uint32_t bytes_read, bytes_wrote;

	for (;;) {
		res = f_read(image_fp, buffer, sizeof buffer, &bytes_read);
		if (res != RES_OK || bytes_read == 0) {
			break;
		}

		bytes_wrote = fwrite(buffer, 1, bytes_read, host_file);
		if (bytes_wrote < bytes_read) {
			printf("Error: could only write %d bytes instead of %d\n", bytes_wrote, bytes_read);
			exit_code = -1;
			break;
		}
	}

	return exit_code;
}

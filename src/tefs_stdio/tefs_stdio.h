/******************************************************************************/
/**
@file		tefs_stdio.h
@author     Wade Penson
@date		June, 2015
@brief      TEFS C file interface header.

@copyright  Copyright 2015 Wade Penson

@license    Licensed under the Apache License, Version 2.0 (the "License");
            you may not use this file except in compliance with the License.
            You may obtain a copy of the License at

              http://www.apache.org/licenses/LICENSE-2.0

            Unless required by applicable law or agreed to in writing, software
            distributed under the License is distributed on an "AS IS" BASIS,
            WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
            implied. See the License for the specific language governing
            permissions and limitations under the License.
*/
/******************************************************************************/

#ifndef TEFS_STDIO_H_
#define TEFS_STDIO_H_

#include "../tefs/tefs.h"
#include "stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t fpos_t;

typedef struct _SD_File
{
	file_t 		f;
	uint32_t	page_address;
	uint16_t	byte_address;
	int8_t 		eof;
} T_FILE;

T_FILE*
t_fopen(
	char *file_name,
	char *mode
);

int8_t
t_fclose(
	T_FILE *fp
);

int8_t
t_remove(
	char *file_name
);

size_t
t_fwrite(
	void 	*ptr,
	size_t 	size,
	size_t 	count,
	T_FILE 	*fp
);

size_t
t_fread(
	void 	*ptr,
	size_t 	size,
	size_t 	count,
	T_FILE 	*fp
);

int8_t
t_feof(
	T_FILE *fp
);

int8_t
t_fflush(
	T_FILE *fp
);

int8_t
t_fseek(
	T_FILE 		*fp,
	uint32_t 	offset,
	int8_t 		whence
);

int8_t
t_fsetpos(
	T_FILE *fp,
	fpos_t *pos
);

int8_t
t_fgetpos(
	T_FILE *fp,
	fpos_t *pos
);

uint32_t
t_ftell(
	T_FILE *fp
);

void
t_rewind(
	T_FILE *fp
);

#ifdef __cplusplus
}
#endif

#endif /* TEFS_STDIO_H_ */

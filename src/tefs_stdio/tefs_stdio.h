#ifndef TEFS_STDIO_H_
#define TEFS_STDIO_H_

#include "../tefs/tefs.h"
#include "stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t fpos_t;				/**< file position */

typedef struct _SD_File
{
	file_t 		f;		/**< The Arduino SD File object we to use. */
	uint32_t	page_address;
	uint16_t	byte_address;
	int8_t 		eof;
} T_FILE;

/**
@brief		Wrapper around Arduino SD file close method.
@param		file	Pointer to C file struct type associated with an SD
			file object.
*/
//void SD_File_Close(SD_File *file);
int8_t
t_fclose(
	T_FILE *fp
);

//Tests the end-of-file indicator for the given stream.
int8_t
t_feof(
	T_FILE *fp
);

//Flushes the output buffer of a stream.
int8_t
t_fflush(
	T_FILE *fp
);

/**
@brief		Wrapper around Arduino SD file position method.
@param		file	Poin,ter to C file struct type associated with an SD
			file object.
@returns	The position of the cursor within the file.
*/
//unsigned long SD_File_Position(SD_File *file);
//Gets the current file position of the stream and writes it to pos
int8_t
t_fgetpos(
	T_FILE *fp,
	fpos_t *pos
);

/**
@brief		Wrapper around Arduino SD file write method.
@param		file		Pointer to a pointer of the C file structure
				type associated with the C++ SD file object.
				The pointer pointed to by this pointer
				will be set to the newly allocated object.
@param		filepath	String containing path to file (basic filename).
@param		mode		What mode to open the file under.
@returns	A file for reading, or @c NULL if an error occurred.
*/
//int SD_File_Open(SD_File **file, char *filepath, unsigned char mode);
//Opens the filename pointed to by filename using the given mode.
T_FILE *
t_fopen(
	char *file_name,
	char *mode
);


/**
@brief		Wrapper around Arduino SD file read method.
@param		file	Pointer to C file struct type associated with an SD
			file object.
@param		buf	Pointer to the memory segment to be read into.
@param		nbytes	The number of bytes to be read.
@returns	A read status.
		(?)
*/
//int SD_File_Read(SD_File *file, void *buf, uint16_t nbytes);
//Reads data from the given stream into the array pointed to by ptr.
size_t
t_fread(
	void *ptr,
	size_t size,
	size_t count,
	T_FILE *fp
);

/**
@brief		Wrapper around Arduino SD file read method.
@param		file	Pointer to C file struct type associated with an SD
			file object.
@param		pos	The position in the file to seek to.
			(from the beginning?)
@returns	@c 1 for success, @c 0 for failure.
*/
//int SD_File_Seek(SD_File *file, unsigned long pos);
//Sets the file position of the stream to the given offset. The argument offset signifies the number of bytes to seek from the given whence position.
int8_t
t_fseek(
	T_FILE *fp,
	uint32_t offset,
	int8_t whence
);

int8_t
t_fsetpos(
	T_FILE *fp,
	fpos_t *pos
);
//Sets the file position of the given stream to the given position. The argument pos is a position given by the function fgetpos.


//Returns the current file position of the given stream.
uint32_t
t_ftell(
	T_FILE *fp
);
/**
@brief		Wrapper around Arduino SD file write method.
@param		file	Pointer to C file struct type associated with an SD
			file object.
@param		buf	Pointer to the data that is to be written.
@param		size	The number of bytes to be written.
@returns	The number of bytes written.
*/
//size_t SD_File_Write(SD_File *file, const uint8_t *buf, size_t size);
//Writes data from the array pointed to by ptr to the given stream.
size_t
t_fwrite(
	void *ptr,
	size_t size,
	size_t count,
	T_FILE *fp
);

/**
@brief		Wrapper around Arduino SD file remove method.
@param		filepath	The string containing the path to the file.
@returns	@c 1 if the file was removed successfully, @c 0 otherwise.
*/
//int SD_File_Remove(char *filepath);
//Deletes the given filename so that it is no longer accessible.
int8_t
t_remove(
	char *file_name
);

//Seek
//Sets the file position to the beginning of the file of the given stream.
void
t_rewind(
	T_FILE *fp
);

/**
@brief		Check to see if an Arduino SD File exists.
@param		filepath	String containing path to file (basic filename).
@returns	@c 1 if the file exists, @c 0 otherwise.
*/
int8_t
t_file_exists(
	char *file_name
);

#ifdef __cplusplus
}
#endif

#endif /* TEFS_STDIO_H_ */

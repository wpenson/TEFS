#include "tefs_stdio.h"

int8_t
t_fclose(
	T_FILE *fp
)
{
	tefs_close(&fp->f);
	free(fp);

	return 0;
}

T_FILE *
t_fopen(
	char *file_name,
	char *mode
)
{
	T_FILE *fp = (T_FILE *) malloc(sizeof(T_FILE));

#if DEBUG
	printf("Target mode: %.*s\n", 2, mode);
#endif

	/* Open a file for reading. The file must exist. */
	if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) 
	{
#if DEBUG
		printf("Checking for file\n");		
#endif

		/* Check to see if file exists. */
		if (!tefs_exists(file_name))
		{
#if DEBUG	
			printf("File does not exist\n");
#endif
			free(fp);
			return NULL;
		}

#if DEBUG
		printf("File exists\n");
		printf("Attempting file open: %s\n", file_name);
#endif

		if (tefs_open(&(fp->f), file_name))
		{
			free(fp);
			return NULL;
		}

		fp->byte_address = 0;
		fp->page_address = 0;
		fp->eof = 0;

		return fp;
	}
	else if (strcmp(mode, "w") == 0 || strcmp(mode, "wb") == 0)
	{
		/* Create an empty file for writing. If a file with the same name
		   already exists, its content is erased and the file is considered as
		   a new empty file. */
#if DEBUG
		printf("Opening file\n");
#endif

#if DEBUG
		printf("Removing existing file\n");		
#endif

		if (tefs_exists(file_name))
		{
			if (tefs_remove(file_name))
			{
				free(fp);
				return NULL;
			}
		}

#if DEBUG
		printf("Attempting creating of file: %s\n", file_name);		
#endif

		if (tefs_open(&fp->f, file_name))
		{
			free(fp);
			return NULL;
		}

		fp->byte_address = 0;
		fp->page_address = 0;
		fp->eof = 0;

		return fp;
	}
	else if (strstr(mode, "r+") != NULL)
	{
#if DEBUG
		printf("Checking for file\n");		
#endif

		/* Open a file for update both reading and writing. The file must exist. */
		if (!tefs_exists(file_name))
		{
			return NULL;
		}

#if DEBUG
		printf("File exists\n");
		printf("Attempting file open: %s\n", file_name);		
#endif

		if (tefs_open(&fp->f, file_name))
		{
			free(fp);
			return NULL;
		}

		fp->byte_address = 0;
		fp->page_address = 0;
		fp->eof = 0;

		return fp;
	}
	else if (strstr(mode, "w+") != NULL)
	{
		/* Create an empty file for both reading and writing. */
#if DEBUG	
		printf("Opening file\n");
#endif

		if (tefs_exists(file_name))
		{
#if DEBUG
			printf("Removing file\n");
#endif
			if (tefs_remove(file_name))
			{
				free(fp);
				return NULL;
			}
		}

#if DEBUG
		printf("Attempting creation of file: %s\n", file_name);		
#endif

		if (tefs_open(&(fp->f), file_name))
		{
			free(fp);
			return NULL;
		}

		fp->byte_address = 0;
		fp->page_address = 0;
		fp->eof = 0;

		return fp;
	}
	else if (strstr(mode, "a+") != NULL)
	{
#if DEBUG
		printf("Opening file\n");
#endif

		if (tefs_open(&fp->f, file_name))
		{
			free(fp);
			return NULL;
		}

		/* Set pointers to the end of the file. */
		fp->page_address = fp->f.eof_page;
		fp->byte_address = fp->f.eof_byte;
		fp->eof = 1;

		return fp;
	}
	else
	{
		/* Incorrect arguments. */
		free(fp);
		return NULL;
	}

	return NULL;
}

int8_t
t_feof(
	T_FILE *fp
)
{
	if (fp->eof == 1)
	{
#ifdef DEBUG
		printf("EOF\n");
#endif

		/* End of file has been reached. */
		return -1;
	}else
	{
#ifdef DEBUG
		printf("NOT EOF\n");
#endif

		/* End of file has not been reached. */
		return 0;
	}
}

int8_t
t_fflush(
	T_FILE *fp
)
{
	tefs_flush(&fp->f);

	return 0;
}

int8_t
t_fsetpos(
	T_FILE *fp,
	fpos_t *pos
)
{
	uint32_t pos_page = *pos >> 9;
	uint16_t pos_byte = *pos & 511;

	/* Check if position is less than the size of the file. */
	if (pos_page > fp->f.eof_page)
	{
		return -1;
	}
	if (pos_page == fp->f.eof_page && pos_byte > fp->f.eof_byte)
	{
		return -1;
	}
	else if (pos_page == fp->f.eof_page && pos_byte == fp->f.eof_byte)
	{
		fp->eof = 1;
	}
	else
	{
		fp->eof = 0;
	}

	fp->page_address = pos_page;
	fp->byte_address = pos_byte;

	return 0;
}

int8_t
t_fgetpos(
	T_FILE *fp,
	fpos_t *pos
)
{
	*pos = (fp->page_address << 9) + fp->byte_address;

	return 0;
}

size_t
t_fread(
	void *ptr,
	size_t size,
	size_t count,
	T_FILE *fp
)
{
	uint32_t total_num_bytes = size * count;
	uint32_t bytes_read = 0;
	int8_t error;

	while (total_num_bytes - bytes_read >= 512 - fp->byte_address)
	{
		if ((error = tefs_read(&fp->f, fp->page_address, (void *) (((char *) ptr) + bytes_read), 512 - fp->byte_address, fp->byte_address)))
		{
			if (error == TEFS_ERR_EOF)
			{
				fp->eof = 1;
				return bytes_read + (512 - fp->byte_address);
			}

			return bytes_read;
		}

		bytes_read += 512 - fp->byte_address;
		fp->page_address++;
		fp->byte_address = 0;
	}

	if (total_num_bytes - bytes_read > 0)
	{
		if ((error = tefs_read(&fp->f, fp->page_address, (void *) (((char *) ptr) + bytes_read), total_num_bytes - bytes_read, fp->byte_address)))
		{
			if (error == TEFS_ERR_EOF)
			{
				fp->eof = 1;
				return bytes_read + (total_num_bytes - bytes_read);
			}

			return bytes_read;
		}

		fp->byte_address += total_num_bytes - bytes_read;
	}

	return total_num_bytes;
}

int8_t
t_fseek(
	T_FILE *fp,
	uint32_t offset,
	int8_t whence
)
{
	fp->eof = 0;

	if (whence == SEEK_SET || whence == SEEK_CUR)
	{
		//seek from current position
		if (whence == SEEK_CUR)
		{
			offset += (fp->page_address << 9) + fp->byte_address;
		}

		uint32_t offset_page = offset >> 9;
		uint16_t offset_byte = offset & 511;

		/* Check if position is less than the size of the file. */
		if ((offset_page == fp->f.eof_page && offset_byte > fp->f.eof_byte) || offset_page > fp->f.eof_page)
		{
			return -1;
		}
		else if (offset_page == fp->f.eof_page && offset_byte == fp->f.eof_byte)
		{
			fp->eof = 1;
		}
		else
		{
			fp->eof = 0;
		}

		fp->page_address = offset_page;
		fp->byte_address = offset_byte;
	}
	else if (whence == SEEK_END)
	{
		fp->eof = 1;

		if (offset > 0) 
		{
			// Cannot seek past end of file
			return -1;
		}
			
	}
	else
	{
		return -1;
	}

	return 0;
}

uint32_t
t_ftell(
	T_FILE *fp
)
{
	return (fp->page_address << 9) + fp->byte_address;
}

size_t
t_fwrite(
	void *ptr,
	size_t size,
	size_t count,
	T_FILE *fp
)
{
	uint32_t total_num_bytes = size * count;
	uint32_t bytes_read = 0;

	while (total_num_bytes - bytes_read >= 512 - fp->byte_address)
	{
		if (tefs_write(&fp->f, fp->page_address, (void *) (((char *) ptr) + bytes_read), 512 - fp->byte_address, fp->byte_address))
		{
			return bytes_read;
		}

		bytes_read += 512 - fp->byte_address;
		fp->page_address++;
		fp->byte_address = 0;
	}

	if (total_num_bytes - bytes_read > 0)
	{
		if (tefs_write(&fp->f, fp->page_address, (void *) (((char *) ptr) + bytes_read), total_num_bytes - bytes_read, fp->byte_address))
		{
			return bytes_read;
		}

		fp->byte_address += total_num_bytes - bytes_read;
	}

	return total_num_bytes;
}

int8_t
t_remove(
	char *file_name
)
{
	return tefs_remove(file_name);
}

void
t_rewind(
	T_FILE *fp
)
{
	fp->eof = 0;
	fp->page_address = 0;
	fp->byte_address = 0;
}

int8_t
t_file_exists(
	char *file_name
)
{
	return tefs_exists(file_name);
}
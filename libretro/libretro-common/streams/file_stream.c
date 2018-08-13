/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_stream.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32)
#  ifdef _MSC_VER
#    define setmode _setmode
#  endif
#  ifdef _XBOX
#    include <xtl.h>
#    define INVALID_FILE_ATTRIBUTES -1
#  else
#    include <io.h>
#    include <fcntl.h>
#    include <direct.h>
#    include <windows.h>
#  endif
#else
#  if defined(PSP)
#    include <pspiofilemgr.h>
#  endif
#  include <sys/types.h>
#  include <sys/stat.h>
#  if !defined(VITA)
#  include <dirent.h>
#  endif
#  include <unistd.h>
#endif

#ifdef __CELLOS_LV2__
#include <cell/cell_fs.h>
#define O_RDONLY CELL_FS_O_RDONLY
#define O_WRONLY CELL_FS_O_WRONLY
#define O_CREAT CELL_FS_O_CREAT
#define O_TRUNC CELL_FS_O_TRUNC
#define O_RDWR CELL_FS_O_RDWR
#else
#include <fcntl.h>
#endif

#include <streams/file_stream.h>
#include <memmap.h>
#include <retro_miscellaneous.h>

struct RFILE
{
   unsigned hints;
   char *ext;
   long long int size;
#if defined(PSP)
   SceUID fd;
#else

#define HAVE_BUFFERED_IO 1

#define MODE_STR_READ "r"
#define MODE_STR_READ_UNBUF "rb"
#define MODE_STR_WRITE_UNBUF "wb"
#define MODE_STR_WRITE_PLUS "w+"

#if defined(HAVE_BUFFERED_IO)
   FILE *fp;
#endif
#if defined(HAVE_MMAP)
   uint8_t *mapped;
   uint64_t mappos;
   uint64_t mapsize;
#endif
   int fd;
#endif
};

int filestream_get_fd(RFILE *stream)
{
   if (!stream)
      return -1;
#if defined(HAVE_BUFFERED_IO)
   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
      return fileno(stream->fp);
#endif
   return stream->fd;
}

const char *filestream_get_ext(RFILE *stream)
{
   if (!stream)
      return NULL;
   return stream->ext;
}

long long int filestream_get_size(RFILE *stream)
{
   if (!stream)
      return 0;
   return stream->size;
}

void filestream_set_size(RFILE *stream)
{
   if (!stream)
      return;

   filestream_seek(stream, 0, SEEK_SET);
   filestream_seek(stream, 0, SEEK_END);

   stream->size = filestream_tell(stream);

   filestream_seek(stream, 0, SEEK_SET);
}

RFILE *filestream_open(const char *path, unsigned mode, ssize_t len)
{
   int            flags = 0;
   int         mode_int = 0;
#if defined(HAVE_BUFFERED_IO)
   const char *mode_str = NULL;
#endif
   RFILE        *stream = (RFILE*)calloc(1, sizeof(*stream));

   if (!stream)
      return NULL;

   (void)mode_int;
   (void)flags;

   stream->hints = mode;

#ifdef HAVE_MMAP
   if (stream->hints & RFILE_HINT_MMAP && (stream->hints & 0xff) == RFILE_MODE_READ)
      stream->hints |= RFILE_HINT_UNBUFFERED;
   else
#endif
      stream->hints &= ~RFILE_HINT_MMAP;

   switch (mode & 0xff)
   {
      case RFILE_MODE_READ_TEXT:
#if  defined(PSP)
         mode_int = 0666;
         flags    = PSP_O_RDONLY;
#else
#if defined(HAVE_BUFFERED_IO)
         if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
            mode_str = MODE_STR_READ;
#endif
         /* No "else" here */
         flags    = O_RDONLY;
#endif
         break;
      case RFILE_MODE_READ:
#if  defined(PSP)
         mode_int = 0666;
         flags    = PSP_O_RDONLY;
#else
#if defined(HAVE_BUFFERED_IO)
         if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
            mode_str = MODE_STR_READ_UNBUF;
#endif
         /* No "else" here */
         flags    = O_RDONLY;
#endif
         break;
      case RFILE_MODE_WRITE:
#if  defined(PSP)
         mode_int = 0666;
         flags    = PSP_O_CREAT | PSP_O_WRONLY | PSP_O_TRUNC;
#else
#if defined(HAVE_BUFFERED_IO)
         if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
            mode_str = MODE_STR_WRITE_UNBUF;
#endif
         else
         {
            flags    = O_WRONLY | O_CREAT | O_TRUNC;
#ifndef _WIN32
            flags   |=  S_IRUSR | S_IWUSR;
#endif
         }
#endif
         break;
      case RFILE_MODE_READ_WRITE:
#if  defined(PSP)
         mode_int = 0666;
         flags    = PSP_O_RDWR;
#else
#if defined(HAVE_BUFFERED_IO)
         if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
            mode_str = MODE_STR_WRITE_PLUS;
#endif
         else
         {
            flags    = O_RDWR;
#ifdef _WIN32
            flags   |= O_BINARY;
#endif
         }
#endif
         break;
   }

#if  defined(PSP)
   stream->fd = sceIoOpen(path, flags, mode_int);
#else
#if defined(HAVE_BUFFERED_IO)
   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0 && mode_str)
   {
      stream->fp = fopen(path, mode_str);
      if (!stream->fp)
         goto error;
   }
   else
#endif
   {
      /* FIXME: HAVE_BUFFERED_IO is always 1, but if it is ever changed, open() needs to be changed to _wopen() for WIndows. */
      stream->fd = open(path, flags, mode_int);
      if (stream->fd == -1)
         goto error;
#ifdef HAVE_MMAP
      if (stream->hints & RFILE_HINT_MMAP)
      {
         stream->mappos  = 0;
         stream->mapped  = NULL;
         stream->mapsize = filestream_seek(stream, 0, SEEK_END);

         if (stream->mapsize == (uint64_t)-1)
            goto error;

         filestream_rewind(stream);

         stream->mapped = (uint8_t*)mmap((void*)0,
               stream->mapsize, PROT_READ,  MAP_SHARED, stream->fd, 0);

         if (stream->mapped == MAP_FAILED)
            stream->hints &= ~RFILE_HINT_MMAP;
      }
#endif
   }
#endif

#if  defined(PSP)
   if (stream->fd == -1)
      goto error;
#endif

   {
      const char *ld = (const char*)strrchr(path, '.');
      stream->ext    = strdup(ld ? ld + 1 : "");
   }

   filestream_set_size(stream);

   return stream;

error:
   filestream_close(stream);
   return NULL;
}

char *filestream_getline(RFILE *stream)
{
   char* newline     = (char*)malloc(9);
   char* newline_tmp = NULL;
   size_t cur_size   = 8;
   size_t idx        = 0;
   int in            = filestream_getc(stream);

   if (!newline)
      return NULL;

   while (in != EOF && in != '\n')
   {
      if (idx == cur_size)
      {
         cur_size *= 2;
         newline_tmp = (char*)realloc(newline, cur_size + 1);

         if (!newline_tmp)
         {
            free(newline);
            return NULL;
         }

         newline = newline_tmp;
      }

      newline[idx++] = in;
      in             = filestream_getc(stream);
   }

   newline[idx] = '\0';
   return newline; 
}

char *filestream_gets(RFILE *stream, char *s, size_t len)
{
   if (!stream)
      return NULL;
#if defined(HAVE_BUFFERED_IO)
   return fgets(s, (int)len, stream->fp);
#elif  defined(PSP)
   if(filestream_read(stream,s,len)==len)
      return s;
   return NULL;
#else
   return gets(s);
#endif
}

int filestream_getc(RFILE *stream)
{
   char c = 0;
   (void)c;
   if (!stream)
      return 0;
#if defined(HAVE_BUFFERED_IO)
    return fgetc(stream->fp);
#elif  defined(PSP)
    if(filestream_read(stream, &c, 1) == 1)
       return (int)c;
    return EOF;
#else
   return getc(stream->fd);
#endif
}

ssize_t filestream_seek(RFILE *stream, ssize_t offset, int whence)
{
   if (!stream)
      goto error;

#if  defined(PSP)
   if (sceIoLseek(stream->fd, (SceOff)offset, whence) == -1)
      goto error;
#else

#if defined(HAVE_BUFFERED_IO)
   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
      return fseek(stream->fp, (long)offset, whence);
#endif

#ifdef HAVE_MMAP
   /* Need to check stream->mapped because this function is 
    * called in filestream_open() */
   if (stream->mapped && stream->hints & RFILE_HINT_MMAP)
   {
      /* fseek() returns error on under/overflow but allows cursor > EOF for
         read-only file descriptors. */
      switch (whence)
      {
         case SEEK_SET:
            if (offset < 0)
               goto error;

            stream->mappos = offset;
            break;

         case SEEK_CUR:
            if ((offset < 0 && stream->mappos + offset > stream->mappos) ||
                  (offset > 0 && stream->mappos + offset < stream->mappos))
               goto error;

            stream->mappos += offset;
            break;

         case SEEK_END:
            if (stream->mapsize + offset < stream->mapsize)
               goto error;

            stream->mappos = stream->mapsize + offset;
            break;
      }
      return stream->mappos;
   }
#endif

   if (lseek(stream->fd, offset, whence) < 0)
      goto error;

#endif

   return 0;

error:
   return -1;
}

int filestream_eof(RFILE *stream)
{
   size_t current_position = filestream_tell(stream);
   size_t end_position     = filestream_seek(stream, 0, SEEK_END);

   filestream_seek(stream, current_position, SEEK_SET);

   if (current_position >= end_position)
      return 1;
   return 0;
}

ssize_t filestream_tell(RFILE *stream)
{
   if (!stream)
      goto error;
#if  defined(PSP)
  if (sceIoLseek(stream->fd, 0, SEEK_CUR) < 0)
     goto error;
#else
#if defined(HAVE_BUFFERED_IO)
   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
      return ftell(stream->fp);
#endif
#ifdef HAVE_MMAP
   /* Need to check stream->mapped because this function 
    * is called in filestream_open() */
   if (stream->mapped && stream->hints & RFILE_HINT_MMAP)
      return stream->mappos;
#endif
   if (lseek(stream->fd, 0, SEEK_CUR) < 0)
      goto error;
#endif

   return 0;

error:
   return -1;
}

void filestream_rewind(RFILE *stream)
{
   filestream_seek(stream, 0L, SEEK_SET);
}

ssize_t filestream_read(RFILE *stream, void *s, size_t len)
{
   if (!stream || !s)
      goto error;
#if  defined(PSP)
   return sceIoRead(stream->fd, s, len);
#else
#if defined(HAVE_BUFFERED_IO)
   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
      return fread(s, 1, len, stream->fp);
#endif
#ifdef HAVE_MMAP
   if (stream->hints & RFILE_HINT_MMAP)
   {
      if (stream->mappos > stream->mapsize)
         goto error;

      if (stream->mappos + len > stream->mapsize)
         len = stream->mapsize - stream->mappos;

      memcpy(s, &stream->mapped[stream->mappos], len);
      stream->mappos += len;

      return len;
   }
#endif
   return read(stream->fd, s, len);
#endif

error:
   return -1;
}

int filestream_flush(RFILE *stream)
{
#if defined(HAVE_BUFFERED_IO)
   return fflush(stream->fp);
#else
   return 0;
#endif
}

ssize_t filestream_write(RFILE *stream, const void *s, size_t len)
{
   if (!stream)
      goto error;
#if  defined(PSP)
   return sceIoWrite(stream->fd, s, len);
#else
#if defined(HAVE_BUFFERED_IO)
   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
      return fwrite(s, 1, len, stream->fp);
#endif
#ifdef HAVE_MMAP
   if (stream->hints & RFILE_HINT_MMAP)
      goto error;
#endif
   return write(stream->fd, s, len);
#endif

error:
   return -1;
}

int filestream_putc(RFILE *stream, int c)
{
   if (!stream)
      return EOF;

#if defined(HAVE_BUFFERED_IO)
   return fputc(c, stream->fp);
#else
   /* unimplemented */
   return EOF;
#endif
}

int filestream_close(RFILE *stream)
{
   if (!stream)
      goto error;

   free(stream->ext);

#if  defined(PSP)
   if (stream->fd > 0)
      sceIoClose(stream->fd);
#else
#if defined(HAVE_BUFFERED_IO)
   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
      if (stream->fp)
         fclose(stream->fp);
   }
   else
#endif
#ifdef HAVE_MMAP
      if (stream->hints & RFILE_HINT_MMAP)
         munmap(stream->mapped, stream->mapsize);
#endif

   if (stream->fd > 0)
      close(stream->fd);
#endif
   free(stream);

   return 0;

error:
   return -1;
}

/**
 * filestream_read_file:
 * @path             : path to file.
 * @buf              : buffer to allocate and read the contents of the
 *                     file into. Needs to be freed manually.
 *
 * Read the contents of a file into @buf.
 *
 * Returns: number of items read, -1 on error.
 */
int filestream_read_file(const char *path, void **buf, ssize_t *len)
{
   ssize_t ret              = 0;
   ssize_t content_buf_size = 0;
   void *content_buf        = NULL;
   RFILE *file              = filestream_open(path, RFILE_MODE_READ, -1);

   if (!file)
   {
      fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
      goto error;
   }

   if (filestream_seek(file, 0, SEEK_END) != 0)
      goto error;

   content_buf_size = filestream_tell(file);
   if (content_buf_size < 0)
      goto error;

   filestream_rewind(file);

   content_buf = malloc(content_buf_size + 1);

   if (!content_buf)
      goto error;

   ret = filestream_read(file, content_buf, content_buf_size);
   if (ret < 0)
   {
      fprintf(stderr, "Failed to read %s: %s\n", path, strerror(errno));
      goto error;
   }

   filestream_close(file);

   *buf    = content_buf;

   /* Allow for easy reading of strings to be safe.
    * Will only work with sane character formatting (Unix). */
   ((char*)content_buf)[ret] = '\0';

   if (len)
      *len = ret;

   return 1;

error:
   if (file)
      filestream_close(file);
   free(content_buf);
   if (len)
      *len = -1;
   *buf = NULL;
   return 0;
}

/**
 * filestream_write_file:
 * @path             : path to file.
 * @data             : contents to write to the file.
 * @size             : size of the contents.
 *
 * Writes data to a file.
 *
 * Returns: true (1) on success, false (0) otherwise.
 */
bool filestream_write_file(const char *path, const void *data, ssize_t size)
{
   ssize_t ret   = 0;
   RFILE *file   = filestream_open(path, RFILE_MODE_WRITE, -1);
   if (!file)
      return false;

   ret = filestream_write(file, data, size);
   filestream_close(file);

   if (ret != size)
      return false;

   return true;
}

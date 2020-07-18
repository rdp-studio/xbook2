X_ASFLAGS	:= -f elf -I xlibc/include
X_INCDIRS	:= xlibc/include

SRC			:= arch/x86/*.asm arch/wordsize-32/*.c ansi/*.c syslib/*.c stdlib/*.c pthread/*.c time/*.c unistd/*.c math/*.c ctype/*.c socket/*.c arpa/*.c string/*.c environ/*.c path/*.c charset/*.c locale/*.c malloc/*.c
SRC			+= stdio/asprintf.c stdio/feof.c stdio/fgetc.c stdio/fopen.c stdio/fputs.c stdio/fscanf.c stdio/ftell.c stdio/getchar.c stdio/putchar.c stdio/rewind.c stdio/setvbuf.c stdio/sscanf.c stdio/__stdio_read.c stdio/tmpfile.c stdio/vasprintf.c stdio/vsscanf.c stdio/clearerr.c stdio/ferror.c stdio/fgetpos.c stdio/fprintf.c stdio/fread.c stdio/fseek.c stdio/fwrite.c stdio/printf.c stdio/remove.c stdio/scanf.c stdio/snprintf.c stdio/__stdio.c stdio/__stdio_write.c stdio/tmpnam.c stdio/vsnprintf.c stdio/fclose.c stdio/fflush.c stdio/fgets.c stdio/fputc.c stdio/freopen.c stdio/fsetpos.c stdio/getc.c stdio/putc.c stdio/rename.c stdio/setbuf.c stdio/sprintf.c stdio/__stdio_flush.c stdio/system.c stdio/ungetc.c
SRC			+= syslib/syscall.asm
TARGET_TYPE	:= static
NAME		:= xc-0.0.1
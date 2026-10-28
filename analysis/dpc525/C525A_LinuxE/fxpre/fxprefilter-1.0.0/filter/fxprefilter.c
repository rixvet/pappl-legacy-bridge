/*
 *  CUPS add-on module for Fuji Xerox Printers
 *  (C) Copyright Fuji Xerox Co., Ltd. 2005 All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <cups/cups.h>
#include <cups/ppd.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>

#include "paramlist.h"
#include "buflist.h"

#define	DATA_BUF_SIZE	(1024 * 256)
#define	LINE_BUF_SIZE	1024

#define IS_BLANK(c) (c == ' '  || c == '\t')
#define IS_RETURN(c)	(c == '\r' || c == '\n')


#ifdef NEC
#define		VENDORPREFIX		"NC"
#else
#define		VENDORPREFIX		"FX"
#endif

#define		KEY_MAINFILTER   VENDORPREFIX"MainFilter"

char *except_appli[] = {
	//"application",
	//"version",
	"OpenOffice.org",
	"1.0",
	"OpenOffice.org",
	"1.1",
	"StarSuite",
	"7",
	NULL
};

#define	EXPTAP_NUM	(sizeof(except_appli)/sizeof(char*))

static
int read_line(int fd, char *p_buf, int bytes)
{
	static char read_buf[DATA_BUF_SIZE];
	static int buf_bytes = 0;
	static int buf_pos = 0;
	int read_bytes = 0;

	while( bytes > 0 )
	{
		if( buf_bytes == 0 && fd != -1 )
		{
			buf_bytes = read(fd, read_buf, DATA_BUF_SIZE);

			if( buf_bytes > 0 ) {
				buf_pos = 0;
			}
		}

		if( buf_bytes > 0 )
		{
			*p_buf = read_buf[buf_pos++];
			bytes--;
			buf_bytes--;
			read_bytes++;

			if( IS_RETURN(*p_buf) ) {
				break;
			}
			p_buf++;
		}
		else if( buf_bytes < 0 )
		{
			if( errno == EINTR ) {
				continue;
			}
		}
		else {
			break;
		}
	}

	return read_bytes;
}

static
ParamList *get_ps_params(int ifd, BufList **ps_data, FILE *tmpfp)
{
	char read_buf[DATA_BUF_SIZE];
	ParamList *p_list = NULL;
	int begin_page = 0;
	int read_bytes;
	int isExceptionApli = 0;
		   
	while( (read_bytes = read_line(ifd, read_buf, DATA_BUF_SIZE - 1)) > 0 )
	{
		fwrite(read_buf, read_bytes, 1, tmpfp);

		// Retain the PS data in the buffer list.
		BufList *bl = buflist_new(read_buf, read_bytes);

		if( *ps_data == NULL ) {
			*ps_data = bl;
		}
		else {
			buflist_add_tail(*ps_data, bl);
		}

		if( read_bytes > 0 )
		{
			if( read_buf[read_bytes - 1] == '\n' ) {
				read_buf[read_bytes - 1] = '\0';
			}
			else {
				read_buf[read_bytes] = '\0';
			}
		}
		else {
			read_buf[0] = '\0';
		}

		// Parse the printing option per line.
		if( strncmp(read_buf, "%%Creator:", 10) == 0 )
		{
			char key_buf[MAX_KEY_LEN + 1];
			char value_buf[MAX_VALUE_LEN + 1];
			int key_len = 0, value_len = 0;
			char *p_code;
			p_code = read_buf + 10;

			while( *p_code != '\0' )
			{
				if( !IS_BLANK(*p_code)  ) {
					break;
				}
				*p_code++;
			}
			while( *p_code != '\0' )
			{
				if( IS_BLANK(*p_code) || key_len >= MAX_KEY_LEN ) {
					break;
				}
				key_buf[key_len++] = *p_code++;
			}
			while( *p_code != '\0' )
			{
				if( !IS_BLANK(*p_code)  ) {
					break;
				}
				*p_code++;
			}
			while( *p_code != '\0' )
			{
				if( IS_BLANK(*p_code) || value_len >= MAX_VALUE_LEN ) {
					break;
				}
				value_buf[value_len++] = *p_code++;
			}

			if( key_len > 0) {
				key_buf[key_len] = '\0';
			}
			if( value_len > 0 ) {
				value_buf[value_len] = '\0';
			}

			int i;
			for( i = 0 ; i < EXPTAP_NUM ; i+=2 )
			{
				if( except_appli[i] != NULL )
				{
					if ( (strncmp(key_buf, except_appli[i], strlen(except_appli[i])) == 0)
						&& (strncmp(value_buf, except_appli[i+1], strlen(except_appli[i+1])) == 0) )
					{
						isExceptionApli = 1;
					}
				}
			}
			
			if (!isExceptionApli)
			{
				while( (read_bytes = read_line(ifd, read_buf, DATA_BUF_SIZE - 1)) > 0 )
				{
					fwrite(read_buf, read_bytes, 1, tmpfp);
				}
				return 0;
			}
		}
		else if( strncmp(read_buf, "%%BeginFeature:", 15) == 0 )
		{
			char key_buf[MAX_KEY_LEN + 1];
			char value_buf[MAX_VALUE_LEN + 1];
			int key_len = 0;
			int value_len = 0;
			char *p_code;

			p_code = read_buf + 15;

			while( *p_code != '\0' )
				if( *p_code++ == '*' )
					break;
			while( *p_code != '\0' )
			{
				if( IS_BLANK(*p_code) || key_len >= MAX_KEY_LEN )
					break;
				key_buf[key_len++] = *p_code++;
			}
			while( *p_code != '\0' )
			{
				if( !IS_BLANK(*p_code)  )
					break;
				*p_code++;
			}
			while( *p_code != '\0' )
			{
				if( IS_BLANK(*p_code) || value_len >= MAX_VALUE_LEN )
					break;
				value_buf[value_len++] = *p_code++;
			}
			if( key_len > 0 && value_len > 0 )
			{
				key_buf[key_len] = '\0';
				value_buf[value_len] = '\0';
				param_list_add(&p_list, key_buf, value_buf, value_len + 1);
			}
		}
		else if( !begin_page && strncmp(read_buf, "%%Page:", 7) == 0 )
		{
			begin_page = 1;
		}
		else if( begin_page )
		{
			if( strncmp(read_buf, "%%EndPageSetup", 14) == 0 )
				break;
			else if( strncmp(read_buf, "gsave", 5) == 0 )
				break;
			else if( read_buf[0] >= '0' && read_buf[0] <= '9' )
				break;
		}
	}

	while( (read_bytes = read_line(ifd, read_buf, DATA_BUF_SIZE - 1)) > 0 )
	{
	    fwrite(read_buf, read_bytes, 1, tmpfp);
	}

	if (!isExceptionApli) {
		p_list = NULL;
	}

	return p_list;
}

static
int get_ppd_params(char *keyname, char *paramname)
{
	FILE*   ppdFile = fopen(getenv("PPD"), "r");
	char	buf[LINE_BUF_SIZE];
	char	*tmpbuf = NULL;
	int     found = 0;

 	if (ppdFile == NULL) {
		return (-1);
	}

	*paramname = '\0';

	while (fgets(buf, sizeof(buf), ppdFile))
	{
		if (*buf != '*')
			continue;
		if (*(buf + 1) == '%')
			continue;

		if (strncmp((buf + 1), keyname, strlen(keyname)) == 0)
		{
			tmpbuf = (buf + 1 + strlen(keyname));
			int foundmark = 0, valuelen = 0;
			for (; *tmpbuf != '\0'; tmpbuf++)
			{
				if (*tmpbuf == '"') {
					foundmark = 1;
				}
				else
				{
					if (foundmark)
					{
						strncat(paramname, tmpbuf, 1);
						valuelen++;
					}
				}
			}
			
			paramname[valuelen - 1] = '\0';			
			found = 1;
			break;
		}
	}

	if (!found) {
		fprintf(stderr, "ERROR: fxprefilter do not found key(%s) or value.\n", keyname);
		return 1;
	}

	if (ppdFile)
		fclose(ppdFile);

	return 0;
}

int	main(int argc, char *argv[])
{
    char    mainFilterName[LINE_BUF_SIZE];
    char    tmpfilename[255];
    ParamList *p_ps_param = NULL;
    BufList *p_ps_data = NULL;
    int ifd = 0;

	setbuf(stderr, NULL);

	if( argc < 6 || argc > 7 )
	{
		fputs("ERROR: fxprefilter job-id user title copies options [file]\n", stderr);
		return 1;
	}

	if( argc == 7 )
	{
		if( (ifd = open(argv[6], O_RDONLY)) == -1 )
		{
			fputs("ERROR: can't open file.\n", stderr);
			return 1;
		}
	}
	else {
		ifd = 0;
	}

	if (get_ppd_params(KEY_MAINFILTER, mainFilterName) < 0 )
	{
	    fputs("ERROR: can't open ppd file.\n", stderr);
	    return 1;
	}
	
	int tempfd = cupsTempFd(tmpfilename, sizeof(tmpfilename));
	FILE *tmpfp = fdopen(tempfd, "wb+");
	
	p_ps_param = get_ps_params(ifd, &p_ps_data, tmpfp);
	fclose(tmpfp);
	
	int num = param_list_num(p_ps_param);
	if( num > 0 )
	{
		char options[DATA_BUF_SIZE];
		int options_len = 0;
		int i;
		for( i = 0 ; i < num ; i++ )
		{
			options_len += (strlen(p_ps_param->key) + strlen(p_ps_param->value) + 2);
			if (options_len > DATA_BUF_SIZE)
			    break;
			strcat(options, p_ps_param->key);
			strcat(options, "=");
			strcat(options, p_ps_param->value);
			if (i < (num - 1)) {
				strcat(options, " ");
			}
			p_ps_param = p_ps_param->next;
		}
		argv[5] = (char *)options;
	}

	pid_t	pid = -1;
	int     status = 0;

	if ((pid = fork()) < 0) {
		return (-1);
	}
	else if (pid == 0)
	{
	    argv[6] = (char *)tmpfilename;
	    execl((char *)mainFilterName, argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], NULL);
	    fprintf(stderr, "ERROR: fxprefilter can not exec filter %s.(errno = %d.)\n", mainFilterName, errno);
	    return (errno);
	}

	wait(&status);

	if ((WIFEXITED(status) && WEXITSTATUS(status) != 0) || (WIFSIGNALED(status))) {
		    kill(pid, SIGTERM);
	}

	if( p_ps_param != NULL )
		param_list_free(p_ps_param);
	if( p_ps_data != NULL )
		buflist_destroy(p_ps_data);
	if( ifd != 0 )
		close(ifd);
	if( tmpfp != 0 )
	    unlink(tmpfilename);

	return 0;
}


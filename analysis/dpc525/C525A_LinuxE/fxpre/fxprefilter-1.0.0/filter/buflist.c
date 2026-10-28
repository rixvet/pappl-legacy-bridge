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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#include "buflist.h"


BufList *buflist_new(unsigned char *buf, int size)
{
	BufList *buf_list = (BufList*)malloc(sizeof(BufList));

	if( buf_list != NULL )
	{
		if( buf != NULL && size != 0 )
		{
			unsigned char *p_data = (unsigned char*)malloc(size);

			if( p_data != NULL )
			{
				memcpy(p_data, buf, size);
				buf_list->next = NULL;
				buf_list->data = p_data;
				buf_list->size = size;

				return buf_list;
			}
		}
		else
		{
			buf_list->next = NULL;
			buf_list->data = NULL;
			buf_list->size = 0;

			return buf_list;
		}

		if( buf_list )
			free(buf_list);
	}

	return NULL;
}

void buflist_destroy(BufList *buf_list)
{
	BufList *p_list = buf_list;
	BufList *p_next;

	if( p_list != NULL )
	{
		do
		{
			p_next = p_list->next;

			if( p_list->data != NULL )
				free(p_list->data);

			free(p_list);
		}
		while( (p_list = p_next) != NULL );
	}
}

unsigned char *buflist_data(BufList *buf_list)
{
	if( buf_list != NULL )
		return buf_list->data;
	else
		return NULL;
}

int buflist_size(BufList *buf_list)
{
	if( buf_list != NULL )
		return buf_list->size;
	else
		return -1;
}

BufList *buflist_tail(BufList *buf_list)
{
	BufList *p_list = buf_list;

	while( p_list != NULL )
	{
		if( p_list->next == NULL )
			break;

		p_list = p_list->next;
	}

	return p_list;
}

BufList *buflist_add_tail(BufList *buf_list, BufList *buf_tail)
{
	BufList *p_list = buflist_tail(buf_list);

	if( p_list != NULL )
		p_list->next = buf_tail;

	return buf_list;
}

int buflist_write(BufList* buf_list, int fd)
{
	BufList *p_list = buf_list;
	int write_bytes = 0;

	while( p_list != NULL )
	{
		if( p_list->data != NULL )
		{
			int bytes = write(fd, p_list->data, p_list->size);

			if( bytes >= 0 )
				write_bytes += bytes;
			else return bytes;
		}
		p_list = p_list->next;
	}

	return write_bytes;
}



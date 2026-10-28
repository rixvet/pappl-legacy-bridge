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

#ifndef _buflist_
#define _buflist_


typedef struct buf_list_s
{
	struct buf_list_s *next;
	unsigned char *data;
	int size;
} BufList;

BufList *buflist_new(unsigned char *buf, int size);
void buflist_destroy(BufList *buf_list);

unsigned char *buflist_data(BufList* buf_list);
int buflist_size(BufList* buf_list);
BufList *buflist_tail(BufList* buf_list);
BufList *buflist_add_tail(BufList *buf_list, BufList *buf_tail);

int buflist_write(BufList* buf_list, int fd);

#endif


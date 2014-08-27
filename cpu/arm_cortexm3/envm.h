/*
 * (C) Copyright 2010,2011
 * Vladimir Khusainov, Emcraft Systems, vlad@emcraft.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef __ENVM_H__
#define __ENVM_H__

/*
 * Initialize the eNVM interface
 */
void envm_init(void);

/*
 * Write a data buffer to eNVM.
 * Note that we need for this function to reside in RAM since it
 * will be used to self-upgrade U-boot in eNMV.
 */
unsigned int
	__attribute__((section(".ramcode")))
	__attribute__ ((long_call))
	envm_write(unsigned int offset, void * buf, unsigned int size);

#endif /* __ENVM_H__ */

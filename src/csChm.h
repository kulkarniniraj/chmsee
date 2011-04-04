/* -*- Mode: C++; -*- */
/*
 *  Copyright (C) 2011 Ji YongGang <jungleji@gmail.com>
 *
 *  ChmSee is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.

 *  ChmSee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with ChmSee; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef __CS_CHM_H__
#define __CS_CHM_H__

#include "csIChm.h"

#define CS_CHM_CID                                                      \
        { 0x9c9192c2, 0x4aa5, 0x11e0, { 0xa9, 0x34, 0x00, 0x24, 0x1d, 0x8c, 0xf3, 0x71 }}
#define CS_CHM_CONTRACTID "@chmsee/cschm;1"

class csChm : public csIChm
{
public:
        NS_DECL_ISUPPORTS
        NS_DECL_CSICHM

        csChm();

private:
        ~csChm();
        void copyinfo(char **, char *);
        NS_IMETHODIMP getAttribute(char **, char *);

        char *mHomepage;
        char *mBookname;
        char *mHhc;
        char *mHhk;
        int   mLcid;

protected:
        /* additional members */
};

#endif //__CS_CHM_H__

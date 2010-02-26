/*
 *  Copyright (C) 2010 Ji YongGang <jungleji@gmail.com>
 *  Copyright (C) 2009 LI Daobing <lidaobing@gmail.com>
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

#include "config.h"

#include "html-factory.h"
#include "components/html-gecko.h"

CsIhtml *
cs_html_new()
{
        return CS_IHTML(cs_html_gecko_new());
}

gboolean
cs_html_init_system()
{
        return cs_html_gecko_init_system();
}

void
cs_html_shutdown_system()
{
        return cs_html_gecko_shutdown_system();
}

void
cs_html_set_default_lang(int lang)
{
        cs_html_gecko_set_default_lang(lang);
}

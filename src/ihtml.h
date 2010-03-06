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

#ifndef __CS_IHTML_H__
#define __CS_IHTML_H__

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CS_TYPE_IHTML             (cs_ihtml_get_type())
#define CS_IHTML(o)               (G_TYPE_CHECK_INSTANCE_CAST((o), CS_TYPE_IHTML, CsIhtml))
#define IS_CS_IHTML(o)            (G_TYPE_CHECK_INSTANCE_TYPE((o), CS_TYPE_IHTML))
#define CS_IHTML_GET_INTERFACE(i) (G_TYPE_INSTANCE_GET_INTERFACE((i), CS_TYPE_IHTML, CsIhtmlInterface))

typedef struct _CsIhtml           CsIhtml;
typedef struct _CsIhtmlInterface  CsIhtmlInterface;

struct _CsIhtmlInterface
{
        GTypeInterface g_iface;
        
        /* Signals */
        void         (* title_changed)    (CsIhtml *html, const gchar *title);
        void         (* location_changed) (CsIhtml *html, const gchar *location);
        gboolean     (* uri_opened)       (CsIhtml *html, const gchar *uri);
        void         (* context_normal)   (CsIhtml *html);
        void         (* context_link)     (CsIhtml *html, const gchar *link);
        void         (* open_new_tab)     (CsIhtml *html, const gchar *uri);
        void         (* link_message)     (CsIhtml *html, const gchar *link);
        
        /* Virtual Table */
        const gchar *(*get_render_name)   (CsIhtml *html);
        GtkWidget   *(*get_widget)        (CsIhtml *html);
        void         (*open_uri)          (CsIhtml *html, const gchar *uri);

        const gchar *(*get_title)         (CsIhtml *html);
        const gchar *(*get_location)      (CsIhtml *html);

        gboolean     (*can_go_back)       (CsIhtml *html);
        gboolean     (*can_go_forward)    (CsIhtml *html);
        void         (*go_back)           (CsIhtml *html);
        void         (*go_forward)        (CsIhtml *html);

        void         (*copy_selection)    (CsIhtml *html);
        void         (*select_all)        (CsIhtml *html);

        void         (*increase_size)     (CsIhtml *html);
        void         (*decrease_size)     (CsIhtml *html);
        void         (*reset_size)        (CsIhtml *html);

        void         (*clear)             (CsIhtml *html);
};

GType        cs_ihtml_get_type(void);

GtkWidget   *cs_ihtml_get_widget(CsIhtml *);
const gchar *cs_ihtml_get_render_name(CsIhtml *);
void         cs_ihtml_open_uri(CsIhtml *, const gchar *);

const gchar *cs_ihtml_get_title(CsIhtml *);
const gchar *cs_ihtml_get_location(CsIhtml *);

gboolean     cs_ihtml_can_go_back(CsIhtml *);
gboolean     cs_ihtml_can_go_forward(CsIhtml *);
void         cs_ihtml_go_back(CsIhtml *);
void         cs_ihtml_go_forward(CsIhtml *);

void         cs_ihtml_copy_selection(CsIhtml *);
void         cs_ihtml_select_all(CsIhtml *);

void         cs_ihtml_increase_size(CsIhtml *);
void         cs_ihtml_reset_size(CsIhtml *);
void         cs_ihtml_decrease_size(CsIhtml *);

void         cs_ihtml_clear(CsIhtml *);

/* Signals */
void         cs_ihtml_title_changed(CsIhtml *, const gchar *);
void         cs_ihtml_location_changed(CsIhtml *, const gchar *);
gboolean     cs_ihtml_uri_opened(CsIhtml *, const gchar *);
void         cs_ihtml_context_normal(CsIhtml *);
void         cs_ihtml_context_link(CsIhtml *, const gchar *);
void         cs_ihtml_open_new_tab(CsIhtml *, const gchar *);
void         cs_ihtml_link_message(CsIhtml *, const gchar *);

G_END_DECLS

#endif /* !__CS_IHTML_H__ */

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

#include "ihtml.h"
#include "marshal.h"

/* Signals */
enum {
        TITLE_CHANGED,
        LOCATION_CHANGED,
        URI_OPENED,
        CONTEXT_NORMAL,
        CONTEXT_LINK,
        OPEN_NEW_TAB,
        LINK_MESSAGE,
        LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

static void cs_ihtml_base_init(gpointer g_class);

GType
cs_ihtml_get_type(void)
{
        static GType type = 0;

        if (!type) {
                static const GTypeInfo info = {
                        sizeof (CsIhtmlInterface),  /* class_size */
                        cs_ihtml_base_init,         /* base_init */
                        NULL,           /* base_finalize */
                        NULL,
                        NULL,		/* class_finalize */
                        NULL,		/* class_data */
                        0,
                        0,              /* n_preallocs */
                        NULL
                };

                type = g_type_register_static(G_TYPE_INTERFACE, "CsIhtml", &info, 0);

                g_type_interface_add_prerequisite(type, GTK_TYPE_OBJECT);
        }

        return type;
}

static void
cs_ihtml_base_init(gpointer g_class)
{
        static gboolean is_initialized = FALSE;

        if (!is_initialized)
        {
                /* signals to the interface */
                signals[TITLE_CHANGED] =
                        g_signal_new("title-changed",
                                     CS_TYPE_IHTML,
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL,
                                     marshal_VOID__STRING,
                                     G_TYPE_NONE,
                                     1, G_TYPE_STRING);
                
                signals[LOCATION_CHANGED] =
                        g_signal_new("location-changed",
                                     CS_TYPE_IHTML,
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL,
                                     marshal_VOID__STRING,
                                     G_TYPE_NONE,
                                     1, G_TYPE_STRING);

                signals[URI_OPENED] =
                        g_signal_new("uri-opened",
                                     CS_TYPE_IHTML,
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL,
                                     marshal_BOOLEAN__STRING,
                                     G_TYPE_BOOLEAN,
                                     1, G_TYPE_STRING);

                signals[CONTEXT_NORMAL] =
                        g_signal_new("context-normal",
                                     CS_TYPE_IHTML,
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL,
                                     marshal_VOID__VOID,
                                     G_TYPE_NONE,
                                     0);

                signals[CONTEXT_LINK] =
                        g_signal_new("context-link",
                                     CS_TYPE_IHTML,
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL,
                                     marshal_VOID__STRING,
                                     G_TYPE_NONE,
                                     1, G_TYPE_STRING);

                signals[OPEN_NEW_TAB] =
                        g_signal_new("open-new-tab",
                                     CS_TYPE_IHTML,
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL,
                                     marshal_VOID__STRING,
                                     G_TYPE_NONE,
                                     1, G_TYPE_STRING);

                signals[LINK_MESSAGE] =
                        g_signal_new("link-message",
                                     CS_TYPE_IHTML,
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL,
                                     marshal_VOID__STRING,
                                     G_TYPE_NONE,
                                     1, G_TYPE_STRING);

                is_initialized = TRUE;
        }
}

GtkWidget *
cs_ihtml_get_widget(CsIhtml *self)
{
        g_return_val_if_fail(IS_CS_IHTML (self), NULL);
        return CS_IHTML_GET_INTERFACE (self)->get_widget(self);
}

void
cs_ihtml_open_uri(CsIhtml *self, const gchar *uri)
{
        g_return_if_fail(IS_CS_IHTML (self));
        g_debug("enter cs_ihtml_open_uri with self=%p, uri=%s", self, uri);
        CS_IHTML_GET_INTERFACE (self)->open_uri(self, uri);
}

const gchar *
cs_ihtml_get_title(CsIhtml *self)
{
        g_return_val_if_fail(IS_CS_IHTML (self), NULL);
        return CS_IHTML_GET_INTERFACE (self)->get_title(self);
}

const gchar *
cs_ihtml_get_location(CsIhtml *self)
{
        g_return_val_if_fail(IS_CS_IHTML (self), NULL);
        return CS_IHTML_GET_INTERFACE (self)->get_location(self);
}

gboolean 
cs_ihtml_can_go_back(CsIhtml *self)
{
        g_return_val_if_fail(IS_CS_IHTML (self), FALSE);
        return CS_IHTML_GET_INTERFACE (self)->can_go_back(self);
}

void
cs_ihtml_go_back(CsIhtml *self)
{
        g_return_if_fail(IS_CS_IHTML (self));
        CS_IHTML_GET_INTERFACE (self)->go_back(self);
}

void
cs_ihtml_go_forward(CsIhtml *self) {
        g_return_if_fail(IS_CS_IHTML (self));
        CS_IHTML_GET_INTERFACE (self)->go_forward(self);
}

gboolean
cs_ihtml_can_go_forward(CsIhtml *self)
{
        g_return_val_if_fail(IS_CS_IHTML (self), FALSE);
        return CS_IHTML_GET_INTERFACE (self)->can_go_forward(self);
}

void
cs_ihtml_copy_selection(CsIhtml *self)
{
        g_return_if_fail(IS_CS_IHTML (self));
        CS_IHTML_GET_INTERFACE (self)->copy_selection(self);
}

void
cs_ihtml_select_all(CsIhtml *self)
{
        g_return_if_fail(IS_CS_IHTML (self));
        CS_IHTML_GET_INTERFACE (self)->select_all(self);
}

void
cs_ihtml_increase_size(CsIhtml *self)
{
        g_return_if_fail(IS_CS_IHTML (self));
        CS_IHTML_GET_INTERFACE (self)->increase_size(self);
}

void
cs_ihtml_reset_size(CsIhtml *self)
{
        g_return_if_fail(IS_CS_IHTML (self));
        CS_IHTML_GET_INTERFACE (self)->reset_size(self);
}

void
cs_ihtml_decrease_size(CsIhtml *self)
{
        g_return_if_fail(IS_CS_IHTML (self));
        CS_IHTML_GET_INTERFACE (self)->decrease_size(self);
}

void
cs_ihtml_clear(CsIhtml *self)
{
        g_return_if_fail(IS_CS_IHTML (self));
        CS_IHTML_GET_INTERFACE (self)->clear(self);
}

/* Emit signals */

void
cs_ihtml_title_changed(CsIhtml *html, const gchar *title)
{
        g_return_if_fail(IS_CS_IHTML (html));
        g_signal_emit(html, signals[TITLE_CHANGED], 0, title);
}

void
cs_ihtml_location_changed(CsIhtml *html, const gchar *location)
{
        g_return_if_fail(IS_CS_IHTML (html));
        g_signal_emit(html, signals[LOCATION_CHANGED], 0, location);
}

gboolean
cs_ihtml_uri_opened(CsIhtml *html, const gchar *uri)
{
        g_return_val_if_fail(IS_CS_IHTML (html), FALSE);
        g_signal_emit(html, signals[URI_OPENED], 0, uri);
        return TRUE; //FIXME:
}

void
cs_ihtml_context_normal(CsIhtml *html)
{
        g_return_if_fail(IS_CS_IHTML (html));
        g_signal_emit(html, signals[CONTEXT_NORMAL], 0);
}

void
cs_ihtml_context_link(CsIhtml *html, const gchar *uri)
{
        g_return_if_fail(IS_CS_IHTML (html));
        g_signal_emit(html, signals[CONTEXT_LINK], 0, uri);
}

void
cs_ihtml_open_new_tab(CsIhtml *html, const gchar *uri)
{
        g_return_if_fail(IS_CS_IHTML (html));
        g_signal_emit(html, signals[OPEN_NEW_TAB], 0, uri);
}

void
cs_ihtml_link_message(CsIhtml *html, const gchar *uri)
{
        g_return_if_fail(IS_CS_IHTML (html));
        g_signal_emit(html, signals[LINK_MESSAGE], 0, uri);
}

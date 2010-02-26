/*
 *  Copyright (C) 2010 Ji YongGang <jungleji@gmail.com>
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

#include <string.h>

#include "html-gecko.h"
#include "ihtml.h"
#include "marshal.h"
#include "utils.h"
#include "gecko-utils.h"

static void cs_html_gecko_class_init(HtmlGeckoClass *);
static void cs_html_gecko_init(HtmlGecko *);

static void gecko_title_cb(GtkMozEmbed *, HtmlGecko *);
static void gecko_location_cb(GtkMozEmbed *, HtmlGecko *);
static gboolean gecko_open_uri_cb(GtkMozEmbed *, const gchar *, HtmlGecko *);
static gboolean gecko_mouse_click_cb(GtkMozEmbed *, gpointer, HtmlGecko *);
static gboolean gecko_link_message_cb(GtkMozEmbed *, HtmlGecko *);
static void gecko_child_add_cb(GtkMozEmbed *, GtkWidget *, HtmlGecko *);
static void gecko_child_remove_cb(GtkMozEmbed *, GtkWidget *, HtmlGecko *);
static void child_grab_focus_cb(GtkWidget *, HtmlGecko *);

/* The value of the URL under the mouse pointer, or NULL */
static gchar *current_url = NULL;

static void cs_ihtml_interface_init (CsIhtmlInterface *iface);

/* GObject functions */

G_DEFINE_TYPE_WITH_CODE (CsHtmlGecko, cs_html_gecko, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CS_TYPE_IHTML, cs_html_gecko_interface_init));

static void
cs_html_gecko_class_init(CsHtmlGeckoClass *klass)
{
}

static void
cs_html_gecko_init(CsHtmlGecko *html)
{
        html->gecko = GTK_MOZ_EMBED(gtk_moz_embed_new());

        gtk_drag_dest_unset(GTK_WIDGET(html->gecko));

        g_signal_connect(G_OBJECT (html->gecko),
                         "title",
                         G_CALLBACK (gecko_title_cb),
                         html);
        g_signal_connect(G_OBJECT (html->gecko),
                         "location",
                         G_CALLBACK (gecko_location_cb),
                         html);
        g_signal_connect(G_OBJECT (html->gecko),
                         "open-uri",
                         G_CALLBACK (gecko_open_uri_cb),
                         html);
        g_signal_connect(G_OBJECT (html->gecko),
                         "dom_mouse_click",
                         G_CALLBACK (gecko_mouse_click_cb),
                         html);
        g_signal_connect(G_OBJECT (html->gecko),
                         "link_message",
                         G_CALLBACK (gecko_link_message_cb),
                         html);
        g_signal_connect(G_OBJECT (html->gecko),
                         "add",
                         G_CALLBACK (gecko_child_add_cb),
                         html);
        g_signal_connect(G_OBJECT (html->gecko),
                         "remove",
                         G_CALLBACK (gecko_child_remove_cb),
                         html);
}

void
cs_html_gecko_interface_init(CsIhtmlInterface *iface)
{
        iface->get_widget = (GtkWidget *(*) (CsIhtml* self)) cs_html_gecko_get_widget;
        iface->open_uri   = (void       (*) (CsIhtml* self, const gchar* uri)) cs_html_gecko_open_uri;

        iface->get_title    = (const gchar *(*) (CsIhtml* self)) cs_html_gecko_get_title;
        iface->get_location = (const gchar *(*) (CsIhtml* self)) cs_html_gecko_get_location;

        iface->can_go_back    = (gboolean (*) (CsIhtml* self)) cs_html_gecko_can_go_back;
        iface->can_go_forward = (gboolean (*) (CsIhtml* self)) cs_html_gecko_can_go_forward;
        iface->go_back        = (void (*) (CsIhtml* self)) cs_html_gecko_go_back;
        iface->go_forward     = (void (*) (CsIhtml* self)) cs_html_gecko_go_forward;

        iface->copy_selection = (void (*) (CsIhtml* self)) cs_html_gecko_copy_selection;
        iface->select_all     = (void (*) (CsIhtml* self)) cs_html_gecko_select_all;

        iface->increase_size = (void (*) (CsIhtml* self)) cs_html_gecko_increase_size;
        iface->decrease_size = (void (*) (CsIhtml* self)) cs_html_gecko_decrease_size;
        iface->reset_size    = (void (*) (CsIhtml* self)) cs_html_gecko_reset_size;

        iface->set_variable_font = (void (*) (CsIhtml* self, const gchar* font)) cs_html_gecko_set_variable_font;
        iface->set_fixed_font    = (void (*) (CsIhtml* self, const gchar* font)) cs_html_gecko_set_fixed_font;

        iface->clear    = (void (*) (CsIhtml* self)) cs_html_gecko_clear;
        iface->shutdown = (void (*) (CsIhtml* self)) cs_html_gecko_shutdown;
}

/* Callbacks */

static void
gecko_title_cb(GtkMozEmbed *embed, CsHtmlGecko *html)
{
        char *new_title;

        new_title = gtk_moz_embed_get_title(embed);
        cs_ihtml_titled_changed(html, new_title);
        g_free(new_title);
}

static void
gecko_location_cb(GtkMozEmbed *embed, CsHtmlGecko *html)
{
        char * location = gtk_moz_embed_get_location(embed);
        cs_ihtml_location_changed(html, location);
        g_free(location);
}

static gboolean
gecko_open_uri_cb(GtkMozEmbed *embed, const gchar *uri, CsHtmlGecko *html)
{
        gboolean ret_val = TRUE;

        /* g_signal_emit(html, signals[OPEN_URI], 0, uri, &ret_val); */ //FIXME: return value
        cs_ihtml_open_uri(html, uri);

        /* reset current url */
        if (current_url != NULL) {
                g_free(current_url);
                current_url = NULL;

                cs_ihtml_link_message(html, "");
        }

        return ret_val;
}

static gboolean
gecko_mouse_click_cb(GtkMozEmbed *widget, gpointer dom_event, CsHtmlGecko *html)
{
        gint button = gecko_utils_get_mouse_event_button(dom_event);
        gint mask   = gecko_utils_get_mouse_event_modifiers(dom_event);

        if (button == 2 || (button == 1 && mask & GDK_CONTROL_MASK)) {
                if (current_url) {
                        cs_ihtml_open_new_tab(html, current_url);
                        return TRUE;
                }
        } else if (button == 3) {
                if (current_url)
                        cs_ihtml_context_link(html, current_url);
                else
                        cs_ithml_context_normal(html);
                return TRUE;
        }

        return FALSE;
}

static gboolean
gecko_link_message_cb(GtkMozEmbed *widget, CsHtmlGecko *html)
{
        g_free(current_url);

        current_url = gtk_moz_embed_get_link_message(widget);
        cs_ihtml_link_message(html, current_url);

        if (current_url[0] == '\0') {
                g_free(current_url);
                current_url = NULL;
        }

        return FALSE;
}

static void
gecko_child_add_cb(GtkMozEmbed *embed, GtkWidget *child, CsHtmlGecko *html)
{
        g_signal_connect(G_OBJECT (child),
                         "grab-focus",
                         G_CALLBACK (html_child_grab_focus_cb),
                         html);
}

static void
gecko_html_child_remove_cb(GtkMozEmbed *embed, GtkWidget *child, CsHtmlGecko *html)
{
        g_signal_handlers_disconnect_by_func(child, html_child_grab_focus_cb, html);
}

static void
gecko_child_grab_focus_cb(GtkWidget *widget, CsHtmlGecko *html)
{
        GdkEvent *event;

        event = gtk_get_current_event();

        if (event == NULL)
                g_signal_stop_emission_by_name(widget, "grab-focus");
        else
                gdk_event_free(event);
}

/* External functions */

CsHtmlGecko *
cs_html_gecko_new(void)
{
        CsHtmlGecko *html;

        html = g_object_new(CS_TYPE_HTML_GECKO, NULL);

        return html;
}

GtkWidget *
cs_html_gecko_get_widget(CsHtmlGecko *html)
{
        g_return_val_if_fail(IS_CS_HTML_GECKO (html), NULL);

        return GTK_WIDGET (html->gecko);
}

void
cs_html_gecko_open_uri(CsHtmlGecko *self, const gchar *str_uri)
{
        gchar *full_uri;

        g_return_if_fail(IS_HTML (self));
        g_return_if_fail(str_uri != NULL);

        if (str_uri[0] == '/')
                full_uri = g_strdup_printf("file://%s", str_uri);
        else
                full_uri = g_strdup(str_uri);

        if (g_strcmp0(full_uri, html_get_location(self)) != 0) {
                g_debug("Open uri %s", full_uri);
                gtk_moz_embed_load_url(self->gecko, full_uri);
        }
        g_free(full_uri);
}

gchar *
cs_html_gecko_get_title(CsHtmlGecko *html)
{
        g_return_val_if_fail(IS_CS_HTML_GECKO (html), NULL);

        return gtk_moz_embed_get_title(html->gecko);
}

gchar *
cs_html_gecko_get_location(CsHtmlGecko *html)
{
        g_return_val_if_fail(IS_CS_HTML_GECKO (html), NULL);

        return gtk_moz_embed_get_location(html->gecko);
}

gboolean
cs_html_gecko_can_go_back(CsHtmlGecko *html)
{
        g_return_val_if_fail(IS_CS_HTML_GECKO (html), FALSE);

        return gtk_moz_embed_can_go_back(html->gecko);
}

gboolean
cs_html_gecko_can_go_forward(CsHtmlGecko *html)
{
        g_return_val_if_fail(IS_CS_HTML_GECKO (html), FALSE);

        return gtk_moz_embed_can_go_forward(html->gecko);
}

void
cs_html_gecko_go_back(CsHtmlGecko *html)
{
        g_return_if_fail(IS_CS_HTML_GECKO (html));

        gtk_moz_embed_go_back(html->gecko);
}

void
cs_html_gecko_go_forward(CsHtmlGecko *html)
{
        g_return_if_fail(IS_CS_HTML_GECKO (html));

        gtk_moz_embed_go_forward(html->gecko);
}

void
cs_html_gecko_copy_selection(CsHtmlGecko *html)
{
        g_return_if_fail(IS_CS_HTML_GECKO (html));

        gecko_utils_copy_selection(html->gecko);
}

void
cs_html_gecko_select_all(CsHtmlGecko *html)
{
        g_return_if_fail(IS_CS_HTML_GECKO (html));

        gecko_utils_select_all(html->gecko);
}

void
cs_html_gecko_increase_size(CsHtmlGecko *html)
{
        gfloat zoom;

        g_return_if_fail(IS_CS_HTML_GECKO (html));

        zoom = gecko_utils_get_zoom(html->gecko);
        zoom *= 1.2;

        gecko_utils_set_zoom(html->gecko, zoom);
}

void
cs_html_gecko_decrease_size(CsHtmlGecko *html)
{
        gfloat zoom;

        g_return_if_fail(IS_CS_HTML_GECKO (html));

        zoom = gecko_utils_get_zoom(html->gecko);
        zoom /= 1.2;

        gecko_utils_set_zoom(html->gecko, zoom);
}

void
cs_html_gecko_reset_size(CsHtmlGecko *html)
{
        g_return_if_fail(IS_CS_HTML_GECKO (html));

        gecko_utils_set_zoom(html->gecko, 1.0);
}

void
cs_cs_html_gecko_gecko_clear(CsHtmlGecko *html)
{
        static const char *data = "<html><body bgcolor=\"white\"></body></html>";

        g_return_if_fail(IS_CS_HTML_GECKO (html));

        gtk_moz_embed_render_data(html->gecko, data, strlen(data), "file:///", "text/html");
}

gboolean 
cs_html_init_system(void)
{
        return gecko_utils_init();
}

void
cs_html_gecko_shutdown_system()
{
        gecko_utils_shutdown();
}

void
cs_html_gecko_set_default_lang(gint lang)
{
        gecko_utils_set_default_lang(lang);
}

void
cs_html_gecko_set_variable_font(CsHtmlGecko *html, const gchar* font)
{
        gecko_utils_set_font(GECKO_PREF_FONT_VARIABLE, font);
}

void
cs_html_gecko_set_fixed_font(CsHtmlGecko *html, const gchar* font)
{
        gecko_utils_set_font(GECKO_PREF_FONT_FIXED, font);
}



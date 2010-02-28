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

#include "config.h"

#include <stdio.h>
#include <string.h>

#include "bookmarks.h"
#include "treeview.h"
#include "utils.h"

/* Signals */
enum {
        LINK_SELECTED,
        LAST_SIGNAL
};

typedef struct _CsBookmarksPrivate CsBookmarksPrivate;

struct _CsBookmarksPrivate {
        GtkWidget     *treeview;
        GtkWidget     *entry;
        GtkWidget     *add_button;
        GtkWidget     *remove_button;

        GList         *links;
        gchar         *current_uri;
};

static gint signals[LAST_SIGNAL] = { 0 };

static void cs_bookmarks_init(CsBookmarks *);
static void cs_bookmarks_class_init(CsBookmarksClass *);
static void cs_bookmarks_finalize(GObject *);

static void link_selected_cb(CsBookmarks *, Link *);
static void entry_changed_cb(GtkEntry *, CsBookmarks *);
static void on_bookmarks_add(CsBookmarks *);
static void on_bookmarks_remove(CsBookmarks *);

static gint link_uri_compare(gconstpointer, gconstpointer);

#define CS_BOOKMARKS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), CS_TYPE_BOOKMARKS, CsBookmarksPrivate))

/* GObject functions */

G_DEFINE_TYPE (CsBookmarks, cs_bookmarks, GTK_TYPE_VBOX);

static void
cs_bookmarks_class_init(CsBookmarksClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        g_type_class_add_private(klass, sizeof(CsBookmarksPrivate));

        object_class->finalize = cs_bookmarks_finalize;

        signals[LINK_SELECTED] =
                g_signal_new("link_selected",
                             G_TYPE_FROM_CLASS (klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET (CsBookmarksClass, link_selected),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__POINTER,
                             G_TYPE_NONE,
                             1,
                             G_TYPE_POINTER);
}

static void
cs_bookmarks_init(CsBookmarks *self)
{
        CsBookmarksPrivate *priv = CS_BOOKMARKS_GET_PRIVATE (self);

        priv->current_uri = NULL;
        priv->links = NULL;

        /* bookmarks list */
        GtkWidget *frame = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME (frame), GTK_SHADOW_IN);

        GtkWidget *list_sw = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (list_sw),
                                       GTK_POLICY_NEVER,
                                       GTK_POLICY_AUTOMATIC);

        priv->treeview = cs_tree_view_new();

        g_signal_connect_swapped(priv->treeview,
                                 "link_selected",
                                 G_CALLBACK (link_selected_cb),
                                 self);

        gtk_container_add(GTK_CONTAINER (list_sw), priv->treeview);
        gtk_container_add(GTK_CONTAINER (frame), list_sw);
        gtk_box_pack_start(GTK_BOX (self), frame, TRUE, TRUE, 0);

        /* bookmark title */
        priv->entry = gtk_entry_new();

        g_signal_connect(priv->entry,
                         "changed",
                         G_CALLBACK (entry_changed_cb),
                         self);

        gtk_box_pack_start(GTK_BOX (self), priv->entry, FALSE, FALSE, 2);

        /* add and remove button */
        GtkWidget *hbox = gtk_hbox_new(FALSE, 0);

        priv->add_button = gtk_button_new_from_stock(GTK_STOCK_ADD);
        g_signal_connect_swapped(G_OBJECT (priv->add_button),
                                 "clicked",
                                 G_CALLBACK (on_bookmarks_add),
                                 self);

        priv->remove_button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
        g_signal_connect_swapped(G_OBJECT (priv->remove_button),
                                 "clicked",
                                 G_CALLBACK (on_bookmarks_remove),
                                 self);

        gtk_box_pack_end(GTK_BOX (hbox), priv->add_button, TRUE, TRUE, 0);
        gtk_box_pack_end(GTK_BOX (hbox), priv->remove_button, TRUE, TRUE, 0);
        //gtk_widget_set_sensitive(self->add_button, TRUE); FIXME: set sensitive

        gtk_box_pack_start(GTK_BOX (self), hbox, FALSE, FALSE, 2);

        gtk_box_set_spacing(GTK_BOX (self), 2);
        gtk_container_set_border_width(GTK_CONTAINER (self), 2);

        gtk_widget_show_all(GTK_WIDGET (self));
}

static void
cs_bookmarks_finalize(GObject *object)
{
        G_OBJECT_CLASS (cs_bookmarks_parent_class)->finalize(object);
}

/* Callbacks */

static void
link_selected_cb(CsBookmarks *self, Link *link)
{
        if (link) {
                g_debug("CS_BOOKMARKS: Emiting link_selected signal");
                g_signal_emit(self, signals[LINK_SELECTED], 0, link);
        }
}

static void
entry_changed_cb(GtkEntry *entry, CsBookmarks *self)
{
        CsBookmarksPrivate *priv = CS_BOOKMARKS_GET_PRIVATE (self);

        const gchar *name = gtk_entry_get_text(entry);
        gint       length = strlen(name);

        if (length >= 2)
                gtk_widget_set_sensitive(priv->add_button, TRUE);
        else
                gtk_widget_set_sensitive(priv->add_button, FALSE);
}

static void
on_bookmarks_add(CsBookmarks *self)
{
        gchar *name;
        GList *found_link;
        Link  *link;

        CsBookmarksPrivate *priv = CS_BOOKMARKS_GET_PRIVATE (self);

        g_debug("CS_BOOKMARKS: add button clicked");

        name = g_strdup(gtk_entry_get_text(GTK_ENTRY (priv->entry)));

        found_link = g_list_find_custom(priv->links, priv->current_uri, link_uri_compare);

        if (found_link) {
                /* update exist bookmark name */
                link = LINK (found_link->data);
                if (ncase_compare_utf8_string(link->name, name) != 0) {
                        cs_tree_view_remove_link(CS_TREE_VIEW (priv->treeview), link);
                        g_free(link->name);

                        link->name = g_strdup(name);
                        cs_tree_view_add_link(CS_TREE_VIEW (priv->treeview), link);
                }
        } else {
                /* new bookmark */
                link = link_new(LINK_TYPE_PAGE, name, priv->current_uri);
                priv->links = g_list_append(priv->links, link);
                
                cs_tree_view_add_link(CS_TREE_VIEW (priv->treeview), link);
        }

        g_free(name);
}

static void
on_bookmarks_remove(CsBookmarks *self)
{
        CsBookmarksPrivate *priv = CS_BOOKMARKS_GET_PRIVATE (self);

        Link *link = cs_tree_view_get_selected_link(CS_TREE_VIEW (priv->treeview));

        if(g_list_find(priv->links, link)) {
                cs_tree_view_remove_link(CS_TREE_VIEW (priv->treeview), link);

                priv->links = g_list_remove(priv->links, link);
                link_free(link); //FIXME: right place?
        }
}

/* Internal functions */

static gint
link_uri_compare(gconstpointer a, gconstpointer b) //FIXME: move to link.c
{
        return ncase_compare_utf8_string(((Link *)a)->uri, (char *)b);
}

/* External functions */

GtkWidget *
cs_bookmarks_new(void)
{
        CsBookmarks *self = g_object_new(CS_TYPE_BOOKMARKS, NULL);

        return GTK_WIDGET (self);
}

void
cs_bookmarks_set_model(CsBookmarks* self, GList* model)
{
        CsBookmarksPrivate *priv = CS_BOOKMARKS_GET_PRIVATE (self);

        priv->links = model;
        cs_tree_view_set_model(CS_TREE_VIEW (priv->treeview), priv->links);
}

GList *
cs_bookmarks_get_model(CsBookmarks *self)
{
        g_return_val_if_fail(IS_CS_BOOKMARKS (self), NULL);

        return CS_BOOKMARKS_GET_PRIVATE (self)->links;
}

void
cs_bookmarks_set_current_link(CsBookmarks *self, Link *link)
{
        g_return_if_fail(IS_CS_BOOKMARKS (self));

        CsBookmarksPrivate *priv = CS_BOOKMARKS_GET_PRIVATE (self);

        g_debug("CS_BOOKMARKS: set bookmarks entry text = %s", link->name);
        gtk_entry_set_text(GTK_ENTRY (priv->entry), link->name);

        gtk_editable_set_position(GTK_EDITABLE (priv->entry), -1);
        gtk_editable_select_region(GTK_EDITABLE (priv->entry), -1, -1);

        g_free(priv->current_uri);

        priv->current_uri = g_strdup(link->uri);
}

void
cs_bookmarks_grab_focus(CsBookmarks *self)
{
        g_return_if_fail(IS_CS_BOOKMARKS (self));

        gtk_widget_grab_focus(CS_BOOKMARKS_GET_PRIVATE (self)->entry);
}


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

#include "index.h"
#include "treeview.h"
#include "models/link.h"

/* Signals */
enum {
        LINK_SELECTED,
        LAST_SIGNAL
};

typedef _CsIndexPrivate CsIndexPrivate;

struct _CsIndexPrivate {
        GtkWidget *treeview;
        GtkEntry  *filter_entry;

        GList     *index;
};

static gint signals[LAST_SIGNAL] = { 0 };

static void cs_index_dispose(GObject *);
static void cs_index_finalize(GObject *);

static void link_selected_cb(CsIndex *, Link *);
static void filter_changed_cb(GtkEntry *, CsIndex *);

#define CS_INDEX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), CS_TYPE_INDEX, CsIndexPrivate))

G_DEFINE_TYPE (CsIndex, cs_index, GTK_TYPE_VBOX);

static void
cs_index_class_init(CsIndexClass* klass)
{
        g_type_class_add_private(klass, sizeof(CsIndexPrivate));

        G_OBJECT_CLASS (klass)->dispose = cs_index_dispose;
        G_OBJECT_CLASS (klass)->finalize = cs_index_finalize;

        signals[LINK_SELECTED] =
                g_signal_new("link_selected",
                             G_TYPE_FROM_CLASS (klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET (CsIndexClass, link_selected),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__POINTER,
                             G_TYPE_NONE,
                             1,
                             G_TYPE_POINTER);
}

static void
cs_index_init(CsIndex* self)
{
        CsIndexPrivate *priv = CS_INDEX_GET_PRIVATE(self);

        priv->index = NULL;

        priv->filter_entry = gtk_entry_new();
        g_signal_connect(priv->filter_entry,
                         "changed",
                         G_CALLBACK (filter_changed_cb),
                         self);
        gtk_box_pack_start(GTK_BOX (self), priv->filter_entry, FALSE, FALSE, 0);

        GtkWidget* index_sw = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (index_sw),
                                       GTK_POLICY_NEVER,
                                       GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW (index_sw),
                                            GTK_SHADOW_IN);

        priv->treeview = cs_treeview_new();
        g_signal_connect_swapped(priv->treeview,
                                 "link_selected",
                                 G_CALLBACK(link_selected_cb),
                                 self);

        gtk_container_add(GTK_CONTAINER (index_sw), priv->treeview);
        gtk_box_pack_start(GTK_BOX (self), index_sw, TRUE, TRUE, 0);

        gtk_widget_show_all(GTK_WIDGET (self));
}

static void
cs_index_dispose(GObject* object)
{
        CsIndex        *self = CS_INDEX(object);
        CsIndexPrivate *priv = CS_INDEX_GET_PRIVATE(self);

        G_OBJECT_CLASS(cs_index_parent_class)->dispose(object);
}

static void
cs_index_finalize(GObject* object)
{
        G_OBJECT_CLASS(cs_index_parent_class)->finalize(object);
}

/* Callbacks */

static void
link_selected_cb(CsIndex* self, Link* link)
{
        g_signal_emit(self, signals[LINK_SELECTED], 0, link);
}

static void
filter_changed_cb(GtkEntry *entry, CsIndex *self)
{
        CsIndexPrivate *priv = CS_INDEX_GET_PRIVATE (self);

        cs_tree_view_set_filter_string(priv->treeview, gtk_entry_get_text(entry));
}

/* Internal functions */

/* External functions */

GtkWidget *
cs_index_new(ChmIndex* chmIndex)
{
        CsIndex* self = CS_INDEX(g_object_new(CS_TYPE_INDEX, NULL));

        return GTK_WIDGET(self);
}

void
cs_index_set_model(CsIndex *self, GList *model)
{
        CsIndexPrivate *priv = CS_INDEX_GET_PRIVATE(self);

        priv->index = model; //FIXME: reset filter_string?
        cs_tree_view_set_model(priv->treeview, model);
}

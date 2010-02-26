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
 *  along with Chmsee; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <unistd.h>             /* R_OK */

#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include "book.h"
#include "toc.h"
#include "bookmarks.h"
#include "index.h"
#include "ihtml.h"
#include "html-factory.h"
#include "link.h"
#include "utils.h"

#include "models/chmfile-factory.h"

enum {
        CS_STATE_INIT,    /* init state, no book is loaded */
        CS_STATE_LOADING, /* loading state, don't pop up an error window when open homepage failed */
        CS_STATE_NORMAL   /* normal state, one book is loaded */
};

/* Signals */
enum {
        MODEL_CHANGED,
        HTML_CHANGED,
        LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };

enum {
        PROP_0,

        PROP_SIDEPANE_VISIBLE,
        PROP_LINK_MESSAGE
};

typedef struct _CsBookPrivate CsBookPrivate;

struct _CsBookPrivate {
        GtkWidget       *control_notebook;
        GtkWidget       *html_notebook;

        GtkWidget       *toc_page;
        GtkWidget       *index_page;
        GtkWidget       *bookmarks_page;

        GtkActionGroup  *action_group;
        GtkUIManager    *ui_manager;

        guint            scid_default;

        gboolean         has_toc;
        gboolean         has_index;
        gint             hpaned_position;
        gint             lang;

        CsChmfile       *model;

        gchar           *context_menu_link;
        gint             state; /* see enum CS_STATE_* */
        gchar           *link_message;
};

#define CS_BOOK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CS_TYPE_BOOK, CsBookPrivate))

static void cs_book_finalize(GObject *);
static void cs_book_dispose(GObject *);
static void cs_book_set_property(GObject *, guint, const GValue *, GParamSpec *);
static void cs_book_get_property(GObject *, guint, GValue *, GParamSpec *);

static void cs_set_context_menu_link(CsBook *, const gchar *);
static GtkWidget* cs_new_index_page(CsBook *);

static void link_selected_cb(CsBook *, Link *);
static void html_notebook_switch_page_cb(GtkNotebook *, GtkNotebookPage *, guint , CsBook *);
static void html_location_changed_cb(CsIhtml *, const gchar *, CsBook *);
static gboolean html_open_uri_cb(CsIhtml *, const gchar *, CsBook *);
static void html_title_changed_cb(CsIhtml *, const gchar *, CsBook *);
static void html_context_normal_cb(CsIhtml *, CsBook *);
static void html_context_link_cb(CsIhtml *, const gchar *, CsBook *);
static void html_open_new_tab_cb(CsIhtml *, const gchar *, CsBook *);
static void html_link_message_cb(CsIhtml *, const gchar *, CsBook *);

static void on_tab_close(GtkWidget *, CsBook *);
static void on_copy(GtkWidget *, CsBook *);
static void on_copy_page_location(GtkWidget*, CsBook*);
static void on_select_all(GtkWidget *, CsBook *);
static void on_back(GtkWidget *, CsBook *);
static void on_forward(GtkWidget *, CsBook *);
static void on_zoom_in(GtkWidget *, CsBook *);
static void on_context_new_tab(GtkWidget *, CsBook *);
static void on_context_copy_link(GtkWidget *, CsBook *);

static void populate_windows(CsBook *);
static void book_close_current_book(CsBook *);
static void update_tab_title(CsBook *, CsIhtml *);
static void tab_set_title(CsBook *, CsIhtml *, const gchar *);
static void open_homepage(CsBook *);

static const GtkActionEntry entries[] = {
        { "Copy", GTK_STOCK_COPY, "_Copy", "<control>C", NULL, G_CALLBACK(on_copy)},
        { "Back", GTK_STOCK_GO_BACK, "_Back", "<alt>Left", NULL, G_CALLBACK(on_back)},
        { "Forward", GTK_STOCK_GO_FORWARD, "_Forward", "<alt>Right", NULL, G_CALLBACK(on_forward)},
        { "OpenLinkInNewTab", NULL, "Open Link in New _Tab", NULL, NULL, G_CALLBACK(on_context_new_tab)},
        { "CopyLinkLocation", NULL, "_Copy Link Location", NULL, NULL, G_CALLBACK(on_context_copy_link)},
        { "SelectAll", NULL, "Select _All", NULL, NULL, G_CALLBACK(on_select_all)},
        { "CopyPageLocation", NULL, "Copy Page _Location", NULL, NULL, G_CALLBACK(on_copy_page_location)},
        { "OnKeyboardControlEqual", NULL, NULL, "<control>equal", NULL, G_CALLBACK(on_zoom_in)}
};

/* Radio items */
static const GtkRadioActionEntry radio_entries[] = {
};

static const char *ui_description =
        "<ui>"
        "  <popup name='HtmlContextLink'>"
        "    <menuitem action='OpenLinkInNewTab' name='OpenLinkInNewTab'/>"
        "    <menuitem action='CopyLinkLocation'/>"
        "  </popup>"
        "  <popup name='HtmlContextNormal'>"
        "    <menuitem action='Back'/>"
        "    <menuitem action='Forward'/>"
        "    <menuitem action='Copy'/>"
        "    <menuitem action='SelectAll'/>"
        "    <menuitem action='CopyPageLocation'/>"
        "  </popup>"
        "  <accelerator action='OnKeyboardControlEqual'/>"
        "</ui>";


/* GObject functions */

G_DEFINE_TYPE (CsBook, cs_book, GTK_TYPE_HPANED);

void
cs_book_class_init(CsBookClass *klass)
{
        GParamSpec* pspec;

        g_type_class_add_private(klass, sizeof(CsBookPrivate));

        G_OBJECT_CLASS(klass)->finalize = cs_book_finalize;
        G_OBJECT_CLASS(klass)->dispose = cs_book_dispose;

        signals[MODEL_CHANGED] =
                g_signal_new ("model_changed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_POINTER);

        signals[HTML_CHANGED] =
                g_signal_new("html_changed",
                             G_TYPE_FROM_CLASS(klass),
                             G_SIGNAL_RUN_LAST,
                             0,
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__POINTER,
                             G_TYPE_NONE,
                             1,
                             G_TYPE_POINTER);

        G_OBJECT_CLASS(klass)->set_property = cs_book_set_property;
        G_OBJECT_CLASS(klass)->get_property = cs_book_get_property;

        pspec = g_param_spec_boolean("sidepane-visible", NULL, NULL, TRUE, G_PARAM_READWRITE);
        g_object_class_install_property(G_OBJECT_CLASS(klass), PROP_SIDEPANE_VISIBLE, pspec);

        pspec = g_param_spec_string("link-message", NULL, NULL, "", G_PARAM_READABLE);
        g_object_class_install_property(G_OBJECT_CLASS(klass), PROP_LINK_MESSAGE, pspec);
}

static void
cs_book_init(CsBook* self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        priv->lang = 0;
        priv->context_menu_link = NULL;

        priv->model = NULL;
        priv->hpaned_position = -1;
        priv->has_toc = FALSE;
        priv->has_index = FALSE;
        priv->state = CS_STATE_INIT;
        priv->link_message = NULL;

        populate_windows(self);
}

static void
cs_book_finalize(GObject *object)
{
        CsBook* self = CS_BOOK(object);
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        g_free(priv->context_menu_link);

        G_OBJECT_CLASS (cs_book_parent_class)->finalize(object);
}

static void
cs_book_dispose(GObject* gobject)
{
        CsBook* self = CS_BOOK(gobject);
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        g_object_unref(priv->model);
        g_object_unref(priv->action_group);
        g_object_unref(priv->ui_manager);

        G_OBJECT_CLASS(cs_book_parent_class)->dispose(gobject);
}

/* Callbacks */

static void
link_selected_cb(CsBook *self, Link *link)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        g_debug("CS_BOOK: link selected: %s", link->uri);
        if (!g_ascii_strcasecmp(CS_NO_LINK, link->uri))
                return;

        CsIhtml *html = cs_book_get_active_html(self);

        g_signal_handlers_block_by_func(html, on_html_open_uri, self);
        cs_ihtml_open_uri(html, g_build_filename(cs_ichmfile_get_dir(priv->model), link->uri, NULL));
        g_signal_handlers_unblock_by_func(html, on_html_open_uri, self);
}

static void
html_notebook_switch_page_cb(GtkNotebook *notebook, GtkNotebookPage *page, guint new_page_num, CsBook *self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        GtkWidget *new_page = gtk_notebook_get_nth_page(notebook, new_page_num);

        if (new_page) {
                CsIhtml* new_html;
                const gchar* title;
                const gchar* location;

                new_html = g_object_get_data(G_OBJECT (new_page), "html");

                update_tab_title(self, new_html);

                title = cs_ihtml_get_title(new_html);
                location = cs_ihtml_get_location(new_html);

                if (location != NULL && strlen(location)) {
                        if (title && title[0]) { //FXIME: What?
                                ui_bookmarks_set_current_link(UIBOOKMARKS (priv->bookmark_page), title, location);
                        } else {
                                const gchar *book_title;

                                book_title = booktree_get_selected_book_title(BOOKTREE (priv->ui_topic));
                                ui_bookmarks_set_current_link(UIBOOKMARKS (priv->bookmark_page), book_title, location);
                        }

                        /* sync link to toc_page */
                        if (priv->has_toc)
                                cs_toc_select_uri(CS_TOC (priv->toc_page), location);
                }
        } else {
                gtk_window_set_title(GTK_WINDOW (self), "CsBook"); //FIXME: gtk_window?
        }

        g_signal_emit(self, signals[HTML_CHANGED], 0, cs_book_get_active_html(self));
}

static void
html_location_changed_cb(CsIhtml *html, const gchar *location, CsBook* self)
{
        g_debug("%s:%d:html location changed cb: %s", __FILE__, __LINE__, location);
        g_signal_emit(self, signals[HTML_CHANGED], 0, cs_book_get_active_html(self));
}

static gboolean
html_open_uri_cb(CsIhtml* html, const gchar *uri, CsBook *self)
{
        g_debug("enter html_open_uri_cb with uri = %s", uri);
        static const char* prefix = "file://";
        static int prefix_len = 7;
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        if(g_str_has_prefix(uri, prefix)) {
                /* FIXME: can't disable the DND function of GtkMozEmbed */
                if(g_str_has_suffix(uri, ".chm")
                   || g_str_has_suffix(uri, ".CHM")) {
                        /* TODO: should popup an event */
                        /* cs_open_uri(self, uri); */
                }

                if(g_access(uri+prefix_len, R_OK) < 0) {
                        g_debug("%s:%d:html_open_uri_cb:%s does not exist", __FILE__, __LINE__, uri+prefix_len);
                        gchar* newfname = correct_filename(uri+prefix_len);
                        if(newfname) {
                                g_message(_("URI redirect: \"%s\" -> \"%s\""), uri, newfname);
                                cs_ihtml_open_uri(html, newfname);
                                g_free(newfname);
                                return TRUE;
                        }

                        if(priv->state == CS_STATE_LOADING) {
                                return TRUE;
                        }
                }
        }

        if ((html == cs_book_get_active_html(self)) && priv->has_toc)
                booktree_select_uri(BOOKTREE (priv->ui_topic), uri);

        return FALSE;
}

static void
html_title_changed_cb(CsIhtml *html, const gchar *title, CsBook *self)
{
        const gchar *location;

        g_debug("html title changed cb %s", title);

        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        update_tab_title(self, cs_book_get_active_html(self));

        location = cs_ihtml_get_location(html);

        if (location != NULL && strlen(location)) {
                if (strlen(title))
                        ui_bookmarks_set_current_link(UIBOOKMARKS (priv->bookmark_page), title, location);
                else {
                        const gchar *book_title;

                        book_title = booktree_get_selected_book_title(BOOKTREE (priv->ui_topic));
                        ui_bookmarks_set_current_link(UIBOOKMARKS (priv->bookmark_page), book_title, location);
                }
        }
}

static void
html_context_normal_cb(CsIhtml *html, CsBook *self)
{
        g_debug("html context-normal event");

        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        gtk_menu_popup(GTK_MENU(gtk_ui_manager_get_widget(priv->ui_manager, "/HtmlContextNormal")),
                       NULL, NULL, NULL, NULL, 0, GDK_CURRENT_TIME);
}

static void
html_context_link_cb(CsIhtml *html, const gchar *link, CsBook* self)
{
        g_debug("html context-link event: %s", link);

        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        cs_set_context_menu_link(self, link);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "OpenLinkInNewTab"),
                                 g_str_has_prefix(priv->context_menu_link, "file://"));

        gtk_menu_popup(GTK_MENU(gtk_ui_manager_get_widget(priv->ui_manager, "/HtmlContextLink")),
                       NULL, NULL, NULL, NULL, 0, GDK_CURRENT_TIME);
}

static void
html_open_new_tab_cb(CsIhtml *html, const gchar *location, CsBook *self)
{
        g_debug("html open new tab callback: %s", location);

        cs_book_new_tab(self, location);
}

static void
html_link_message_cb(CsIhtml *html, const gchar *url, CsBook *self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        g_free(priv->link_message);
        priv->link_message = g_strdup(url);
        g_object_notify(G_OBJECT(self), "link-message");
}

static void
on_tab_close(GtkWidget *widget, CsBook *self)
{
        gint num_pages, number, i;
        GtkWidget *tab_label, *page;
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        number = -1;
        num_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK (priv->html_notebook));

        if (num_pages == 1) {
                /* TODO: should open a new empty tab */
                return;
        }

        for (i = 0; i < num_pages; i++) {
                GList *children, *l;

                g_debug("page %d", i);
                page = gtk_notebook_get_nth_page(GTK_NOTEBOOK (priv->html_notebook), i);

                tab_label = gtk_notebook_get_tab_label(GTK_NOTEBOOK (priv->html_notebook), page);
                g_message("tab_label");
                children = gtk_container_get_children(GTK_CONTAINER (tab_label));

                for (l = children; l; l = l->next) {
                        if (widget == l->data) {
                                g_debug("found tab on page %d", i);
                                number = i;
                                break;
                        }
                }

                if (number >= 0) {
                        gtk_notebook_remove_page(GTK_NOTEBOOK (priv->html_notebook), number);

                        break;
                }
        }
}

static void
on_copy(GtkWidget *widget, CsBook *self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        g_message("On Copy");

        g_return_if_fail(GTK_IS_NOTEBOOK (priv->html_notebook));

        cs_ihtml_copy_selection(cs_book_get_active_html(self));
}

static void
on_copy_page_location(GtkWidget* widget, CsBook* cs)
{
        CsIhtml* html = cs_book_get_active_html(cs);
        const gchar* location = cs_ihtml_get_location(html);
        if (!location)
                return;

        gtk_clipboard_set_text(
                gtk_clipboard_get(GDK_SELECTION_PRIMARY),
                location,
                -1);
        gtk_clipboard_set_text(
                gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
                location,
                -1);
}

static void
on_select_all(GtkWidget *widget, CsBook *self)
{
        CsIhtml *html;

        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        g_message("On Select All");

        g_return_if_fail(GTK_IS_NOTEBOOK (priv->html_notebook));

        html = cs_book_get_active_html(self);
        cs_ihtml_select_all(html);
}

static void
on_back(GtkWidget *widget, CsBook *self)
{
        cs_ihtml_go_back(cs_book_get_active_html(self));
}

static void
on_forward(GtkWidget *widget, CsBook *self)
{
        cs_ihtml_go_forward(cs_book_get_active_html(self));
}

static void
on_zoom_in(GtkWidget *widget, CsBook *self)
{
        CsIhtml* html = cs_book_get_active_html(self);
        if(html != NULL) {
                cs_ihtml_increase_size(html);
        }
}

static void
on_context_new_tab(GtkWidget *widget, CsBook *self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        g_debug("On context open new tab: %s", priv->context_menu_link);

        if (priv->context_menu_link != NULL) {
                cs_book_new_tab(self, priv->context_menu_link);
        }
}

static void
on_context_copy_link(GtkWidget *widget, CsBook *self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        g_debug("On context copy link: %s", priv->context_menu_link);

        if (priv->context_menu_link != NULL) {
                gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY),
                                       priv->context_menu_link, -1);
                gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
                                       priv->context_menu_link, -1);
        }
}

/* Internal functions */

static void
cs_book_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
        CsBook* self = CS_BOOK(object);
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        switch(property_id) {
        case PROP_SIDEPANE_VISIBLE:
                if(g_value_get_boolean(value)) {
                        gtk_widget_show(gtk_paned_get_child1(GTK_PANED(self)));
                } else {
                        gtk_widget_hide(gtk_paned_get_child1(GTK_PANED(self)));
                }
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
cs_book_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
        CsBook* self = CS_BOOK(object);
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        switch(property_id) {
        case PROP_SIDEPANE_VISIBLE:
                g_value_set_boolean(value, GTK_WIDGET_VISIBLE(gtk_paned_get_child1(GTK_PANED(self))));
                break;
        case PROP_LINK_MESSAGE:
                g_value_set_string(value, priv->link_message);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
populate_windows(CsBook *self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        GtkWidget* control_vbox = gtk_vbox_new(FALSE, 0);

        priv->control_notebook = gtk_notebook_new();
        gtk_box_pack_start(GTK_BOX (control_vbox),
                           GTK_WIDGET (control_notebook),
                           TRUE,
                           TRUE,
                           2);

        /* book TOC */
        priv->toc_page = GTK_WIDGET (g_object_ref_sink(cs_toc_new(NULL)));
        gtk_notebook_append_page(GTK_NOTEBOOK (control_notebook),
                                 toc_page,
                                 gtk_label_new(_("Topics")));

        g_signal_connect_swapped(G_OBJECT (toc_page),
                                 "link-selected",
                                 G_CALLBACK (link_selected_cb),
                                 self);

        /* book index */
        priv->index_page = GTK_WIDGET (cs_index_new());
        gtk_notebook_append_page(GTK_NOTEBOOK (control_notebook),
                                 index_page,
                                 gtk_label_new(_("Index")));

        g_signal_connect_swapped(G_OBJECT (index_page),
                                 "link-selected",
                                 G_CALLBACK (link_selected_cb),
                                 self);

        /* bookmarks */
        priv->bookmarks_page = GTK_WIDGET (cs_bookmarks_new(NULL));
        gtk_notebook_append_page(GTK_NOTEBOOK (control_notebook),
                                 priv->bookmarks_page,
                                 gtk_label_new (_("Bookmarks")));

        g_signal_connect_swapped(G_OBJECT (priv->bookmarks_page),
                                 "link-selected",
                                 G_CALLBACK (link_selected_cb),
                                 self);

        gtk_paned_add1(GTK_PANED(self), control_vbox);

        /* HTML page tabs notebook */
        priv->html_notebook = gtk_notebook_new();
        g_signal_connect(G_OBJECT (html_notebook),
                         "switch-page",
                         G_CALLBACK (html_notebook_switch_page_cb),
                         self);

        cs_book_new_tab(self, NULL);

        gtk_paned_add2 (GTK_PANED (self), html_notebook);

        gtk_widget_show_all(GTK_WIDGET(self));

        /* HTML content popup menu */
        GtkActionGroup* action_group = gtk_action_group_new("BookActions");
        priv->action_group = action_group;
        gtk_action_group_add_actions(action_group, entries, G_N_ELEMENTS (entries), self);

        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "Back"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "Forward"), FALSE);

        GtkUIManager* ui_manager = gtk_ui_manager_new();
        priv->ui_manager = ui_manager;
        gtk_ui_manager_insert_action_group(ui_manager, action_group, 0);

        GError* error = NULL;
        if (!gtk_ui_manager_add_ui_from_string(ui_manager, ui_description, -1, &error))
        {
                g_message("CS_BOOK: building menus failed %s", error->message);
                g_error_free (error);
                exit (EXIT_FAILURE);
        }
}

static GtkWidget*
new_tab_label(CsBook *self, const gchar *str)
{
        GtkWidget *hbox  = gtk_hbox_new(FALSE, 3);

        GtkWidget *label = gtk_label_new(str);
        gtk_label_set_ellipsize(GTK_LABEL (label), PANGO_ELLIPSIZE_END);
        gtk_label_set_single_line_mode(GTK_LABEL (label), TRUE);
        gtk_misc_set_alignment(GTK_MISC (label), 0.0, 0.5);
        gtk_misc_set_padding(GTK_MISC (label), 0, 0);
        gtk_box_pack_start(GTK_BOX (hbox), label, TRUE, TRUE, 0);
        g_object_set_data(G_OBJECT (hbox), "label", label);

        GtkWidget *close_button = gtk_button_new();
        gtk_button_set_relief(GTK_BUTTON(close_button), GTK_RELIEF_NONE);

        GtkWidget *close_image = gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
        gtk_container_add(GTK_CONTAINER (close_button), close_image);

        g_signal_connect(G_OBJECT (close_button),
                         "clicked",
                         G_CALLBACK (on_tab_close),
                         self);

        gtk_box_pack_start(GTK_BOX (hbox), close_button, FALSE, FALSE, 0);

        gtk_widget_show_all(hbox);

        return hbox;
}

/* External functions*/

GtkWidget *
cs_book_new()
{
        return GTK_WIDGET(g_object_new(CS_TYPE_BOOK, NULL));
}

void
cs_book_set_model(CsBook* self, CsChmfile *model)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        if(model == NULL)
                return;

        /* close opened book */
        cs_book_close_book(self);

        g_debug("CS_BOOK: display book");

        priv->state = CS_STATE_LOADING;
        priv->model = g_object_ref(model);

        /* TOC */
        GNode *link_tree = cs_ichmfile_get_link_tree(model);

        if (link_tree != NULL) {
                cs_toc_set_model(CS_TOC(priv->toc_page), link_tree);
                gtk_widget_show(priv->topic_page);
                priv->has_toc = TRUE;
        } else {
                gtk_widget_hide(priv->toc_page);
                priv->has_toc = FALSE;
        }

        /* index */
        GList* index_list = cs_ichmfile_get_index(model);

        if(index != NULL) {
                cs_index_set_model(CS_INDEX(priv->index_page), index_list);
                gtk_widget_show(priv->index_page);
                priv->has_index = TRUE;
        } else {
                gtk_widget_hide(priv->index_page);
                priv->has_index = FALSE;
        }

        /* bookmarks */
        GList *bookmarks_list = cs_ichmfile_get_bookmarks_list(model);
        cs_bookmarks_set_model(CS_BOOKMARKS(priv->bookmarks_page), bookmarks_list);

        gtk_notebook_set_current_page(GTK_NOTEBOOK (priv->control_notebook),
                                      g_list_length(bookmarks_list) && priv->has_toc ? 1 : 0); //FIXME: set to bookmars page

        cs_ihtml_set_variable_font(cs_book_get_active_html(self),
                                   cs_ichmfile_get_variable_font(priv->model));
        cs_ihtml_set_fixed_font(cs_book_get_active_html(self),
                                cs_ichmfile_get_fixed_font(priv->model));

        if (cs_chmfile_get_homepage(model)) {
                open_homepage(self);
        }

        priv->state = CS_STATE_NORMAL;

        priv->last_dir = g_strdup_printf("%s", g_path_get_dirname(cs_chmfile_get_filename(model)));

        g_signal_emit(self, signals[MODEL_CHANGED], 0, model);
}

void
cs_book_close_book(CsBook *self)
{
        CsBookPrivate *priv   = CS_BOOK_GET_PRIVATE(self);

        if (priv->model)
                g_object_unref(priv->model);

        priv->model = NULL;
        priv->state = CS_STATE_INIT;
}

void
cs_book_new_tab(CsBook *self, const gchar *location)
{
        g_debug("new_tab : %s", location);

        /* only allow local link */
        if (location != NULL && !g_str_has_prefix(location, "file://"))
                return;

        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        CsIhtml       *html = cs_html_new();
        GtkWidget     *view = cs_ihtml_get_widget(html);

        GtkWidget *page = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME (page), GTK_SHADOW_IN);
        gtk_container_set_border_width(GTK_CONTAINER (page), 2);
        gtk_container_add(GTK_CONTAINER (page), view);

        g_object_set_data(G_OBJECT (page), "html", html);

        g_signal_connect(G_OBJECT (html),
                         "title-changed",
                         G_CALLBACK (html_title_changed_cb),
                         self);
        g_signal_connect(G_OBJECT (html),
                         "open-uri",
                         G_CALLBACK (html_open_uri_cb),
                         self);
        g_signal_connect(G_OBJECT (html),
                         "location-changed",
                         G_CALLBACK (html_location_changed_cb),
                         self);
        g_signal_connect(G_OBJECT (html),
                         "context-normal",
                         G_CALLBACK (html_context_normal_cb),
                         self);
        g_signal_connect(G_OBJECT (html),
                         "context-link",
                         G_CALLBACK (html_context_link_cb),
                         self);
        g_signal_connect(G_OBJECT (html),
                         "open-new-tab",
                         G_CALLBACK (html_open_new_tab_cb),
                         self);
        g_signal_connect(G_OBJECT (html),
                         "link-message",
                         G_CALLBACK (html_link_message_cb),
                         self);

        /* customized label, add a close button rightmost */
        GtkWidget *tab_label = new_tab_label(self, _("No Title"));
        gint page_num = gtk_notebook_append_page(GTK_NOTEBOOK (priv->html_notebook),
                                                 page, tab_label);

        gtk_notebook_set_tab_label_packing(GTK_NOTEBOOK (priv->html_notebook),
                                           page,
                                           TRUE, TRUE,
                                           GTK_PACK_START);

        if (priv->model != NULL && location != NULL) {
                cs_ihtml_open_uri(html, location);

                /* synchronize link with toc */
                if (priv->has_toc)
                        cs_toc_select_uri(CS_TOC (priv->toc_page), location);
        } else {
                /* cs_ihtml_clear(html); */ // FIXME:
        }

        gtk_notebook_set_current_page(GTK_NOTEBOOK (priv->html_notebook), num);

        g_signal_emit(self, signals[HTML_CHANGED], 0, cs_book_get_active_html(self));
}

void
open_homepage(CsBook *self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        CsIhtml       *html = cs_book_get_active_html(self);

        /* g_signal_handlers_block_by_func(html, html_open_uri_cb, self); */

        cs_ihtml_open_uri(html, g_build_filename(cs_ichmfile_get_dir(priv->model),
                                                 cs_ichmfile_get_home(priv->model), NULL));

        /* g_signal_handlers_unblock_by_func(html, html_open_uri_cb, self); */

        if (priv->has_toc) {
                cs_toc_select_uri(CS_TOC (priv->toc_page),
                                  cs_ichmfile_get_home(priv->model));
        }
}

CsIhtml *
cs_book_get_active_html(CsBook *self)
{
        gint page_num;

        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        if(!priv->html_notebook) {
                g_warning("CS_BOOK: cannot find active html! ");
                return NULL;
        }

        page_num = gtk_notebook_get_current_page(GTK_NOTEBOOK (priv->html_notebook));

        if (page_num == -1)
                return NULL;

        GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK (priv->html_notebook), page_num);

        return g_object_get_data(G_OBJECT (page), "html");
}

static void
update_tab_title(CsBook *self, CsIhtml *html)
{
        gchar* html_title;
        const gchar* tab_title;
        const gchar* book_title;

        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        html_title = g_strdup(cs_ihtml_get_title(html));

        if (priv->has_toc)
                book_title = booktree_get_selected_book_title(BOOKTREE (priv->ui_topic));
        else
                book_title = "";

        if (book_title && book_title[0] != '\0' &&
            html_title && html_title[0] != '\0' &&
            ncase_compare_utf8_string(book_title, html_title))
                tab_title = g_strdup_printf("%s : %s", book_title, html_title);
        else if (book_title && book_title[0] != '\0')
                tab_title = g_strdup(book_title);
        else if (html_title && html_title[0] != '\0')
                tab_title = g_strdup(html_title);
        else
                tab_title = g_strdup("");

        tab_set_title(self, html, tab_title);
        g_free(html_title);
}

static void
tab_set_title(CsBook *self, CsIhtml *html, const gchar *title)
{
        GtkWidget *view;
        GtkWidget *page;
        GtkWidget *widget, *label;
        gint num_pages, i;

        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        view = cs_ihtml_get_widget(html);

        if (title == NULL || title[0] == '\0')
                title = _("No Title");

        num_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK (priv->html_notebook));

        for (i = 0; i < num_pages; i++) {
                page = gtk_notebook_get_nth_page(GTK_NOTEBOOK (priv->html_notebook), i);

                if (gtk_bin_get_child(GTK_BIN (page)) == view) {
                        widget = gtk_notebook_get_tab_label(GTK_NOTEBOOK (priv->html_notebook), page);

                        label = g_object_get_data(G_OBJECT (widget), "label");

                        if (label != NULL)
                                gtk_label_set_text(GTK_LABEL (label), title);

                        break;
                }
        }
}

int
cs_book_get_hpaned_position(CsBook* self)
{
        return gtk_paned_get_position(GTK_PANED(self));
}

void
cs_book_set_hpaned_position(CsBook* self, int hpaned_position)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        priv->hpaned_position = hpaned_position;
        /*
          g_object_set(G_OBJECT(get_widget(self, "hpaned1")),
          "position", hpaned_position,
          NULL
          );
        */
}

const gchar *
cs_book_get_cache_dir(CsBook* self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        return priv->cache_dir;
}

const gchar *
cs_book_get_variable_font(CsBook* self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        g_return_val_if_fail(priv->model, NULL);
        return cs_ichmfile_get_variable_font(priv->model);
}

void
cs_book_set_variable_font(CsBook* self, const gchar* font_name)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        g_return_if_fail(priv->model);
        cs_ichmfile_set_variable_font(priv->model, font_name);
}

const gchar *
cs_book_get_fixed_font(CsBook* self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        g_return_val_if_fail(priv->model, NULL);
        return cs_ichmfile_get_fixed_font(priv->model);
}

void
cs_book_set_fixed_font(CsBook* self, const gchar* font_name)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        g_return_if_fail(priv->model);
        cs_ichmfile_set_fixed_font(priv->model, font_name);
}

int
cs_book_get_lang(CsBook* self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        return priv->lang;
}

void
cs_book_set_lang(CsBook* self, int lang)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        priv->lang = lang;
}

gboolean
cs_book_has_book(CsBook* self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        return priv->model != NULL;
}

gboolean
cs_book_jump_index_by_name(CsBook* self, const gchar* name)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        g_return_val_if_fail(CS_IS_BOOK(self), FALSE);

        gboolean res = cs_ui_index_select_link_by_name(
                CS_UI_INDEX(self->priv->ui_index),
                name);

        if(res) {
                /* TODO: hard-code page num 1 */
                gtk_notebook_set_current_page(GTK_NOTEBOOK(self->priv->control_notebook), 1);
        }
        return res;
}

static void
cs_set_context_menu_link(CsBook* self, const gchar* link)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        g_free(priv->context_menu_link);
        priv->context_menu_link = g_strdup(link);
}

void
cs_book_close_current_tab(CsBook* self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        g_return_if_fail(GTK_IS_NOTEBOOK (priv->html_notebook));

        if (gtk_notebook_get_n_pages(GTK_NOTEBOOK (priv->html_notebook)) == 1) {
                /* TODO: should open a new tab */
        }

        gint page_num = gtk_notebook_get_current_page(GTK_NOTEBOOK (priv->html_notebook));

        if (page_num >= 0)
                gtk_notebook_remove_page(GTK_NOTEBOOK (priv->html_notebook), page_num);
}

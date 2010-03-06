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
#include <gtkmozembed.h>
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
#include "html-gecko.h"
#include "utils.h"
#include "models/chmfile.h"
#include "models/link.h"

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
        GtkWidget       *hpaned;
        GtkWidget       *findbar;

        GtkWidget       *control_notebook;
        GtkWidget       *html_notebook;

        GtkWidget       *toc_page;
        GtkWidget       *index_page;
        GtkWidget       *bookmarks_page;

        GtkActionGroup  *action_group;
        GtkUIManager    *ui_manager;

        guint            scid_default;

        gboolean         has_toc;
        gint             lang;

        CsChmfile       *model;

        gchar           *context_menu_link;
        gchar           *link_message;
};

#define CS_BOOK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CS_TYPE_BOOK, CsBookPrivate))

static void cs_book_class_init(CsBookClass *);
static void cs_book_init(CsBook *);
static void cs_book_dispose(GObject *);
static void cs_book_finalize(GObject *);
static void cs_book_set_property(GObject *, guint, const GValue *, GParamSpec *);
static void cs_book_get_property(GObject *, guint, GValue *, GParamSpec *);

static void link_selected_cb(CsBook *, Link *);
static void html_notebook_switch_page_cb(GtkNotebook *, GtkNotebookPage *, guint , CsBook *);
static void html_location_changed_cb(CsIhtml *, const gchar *, CsBook *);
static gboolean html_uri_opened_cb(CsIhtml *, const gchar *, CsBook *);
static void html_title_changed_cb(CsIhtml *, const gchar *, CsBook *);
static void html_context_normal_cb(CsIhtml *, CsBook *);
static void html_context_link_cb(CsIhtml *, const gchar *, CsBook *);
static void html_open_new_tab_cb(CsIhtml *, const gchar *, CsBook *);
static void html_link_message_cb(CsIhtml *, const gchar *, CsBook *);

static void on_tab_close(GtkWidget *, CsBook *);
static void on_copy(GtkWidget *, CsBook *);
static void on_copy_page_location(GtkWidget *, CsBook *);
static void on_select_all(GtkWidget *, CsBook *);
static void on_back(GtkWidget *, CsBook *);
static void on_forward(GtkWidget *, CsBook *);
static void on_zoom_in(GtkWidget *, CsBook *);
static void on_context_new_tab(GtkWidget *, CsBook *);
static void on_context_copy_link(GtkWidget *, CsBook *);
static void on_finder_close(GtkWidget *, CsBook *);

static GtkWidget *new_html_page(CsBook *);
static CsIhtml *get_active_html(CsBook *);
static GtkWidget *new_tab_label(CsBook *, const gchar *);
static void update_tab_title(CsBook *, CsIhtml *);
static void set_context_menu_link(CsBook *, const gchar *);

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

G_DEFINE_TYPE (CsBook, cs_book, GTK_TYPE_VBOX);

static void
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
cs_book_init(CsBook *self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        priv->lang = 0;
        priv->context_menu_link = NULL;

        priv->model = NULL;
        priv->has_toc = FALSE;
        priv->link_message = NULL;

        priv->hpaned = gtk_hpaned_new();
        gtk_box_pack_start(GTK_BOX (self), priv->hpaned, TRUE, TRUE, 0);

        priv->control_notebook = gtk_notebook_new();
        gtk_paned_add1(GTK_PANED(priv->hpaned), priv->control_notebook);

        priv->html_notebook = gtk_notebook_new();
        gtk_paned_add2(GTK_PANED (priv->hpaned), priv->html_notebook);

        /* string find bar */
        priv->findbar = GTK_WIDGET (gtk_hbox_new(FALSE, 2));

        GtkWidget *close_button = gtk_button_new();
        gtk_button_set_relief(GTK_BUTTON(close_button), GTK_RELIEF_NONE);
        GtkWidget *close_image = gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
        gtk_container_add(GTK_CONTAINER (close_button), close_image);

        g_signal_connect(G_OBJECT (close_button),
                         "clicked",
                         G_CALLBACK (on_finder_close),
                         self);
        gtk_box_pack_start(GTK_BOX (priv->findbar), close_button, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX (self), priv->findbar, FALSE, FALSE, 0);

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
                g_warning("CS_BOOK >>> building menus failed %s", error->message);
                g_error_free (error);
                exit (EXIT_FAILURE);
        }

        gtk_box_set_homogeneous(GTK_BOX (self), FALSE);
        gtk_widget_show_all(GTK_WIDGET (self));
        g_debug("CS_BOOK >>> populate windows finished! ");
}

static void
cs_book_dispose(GObject* gobject)
{
        g_debug("CS_BOOK >>> dispose");
        CsBook *self = CS_BOOK(gobject);
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        g_object_unref(priv->model);
        g_object_unref(priv->action_group);
        g_object_unref(priv->ui_manager);

        G_OBJECT_CLASS(cs_book_parent_class)->dispose(gobject);
}

static void
cs_book_finalize(GObject *object)
{
        g_debug("CS_BOOK >>> finalize");
        CsBook *self = CS_BOOK(object);
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        g_free(priv->context_menu_link);

        G_OBJECT_CLASS (cs_book_parent_class)->finalize(object);
}

/* Callbacks */

static void
link_selected_cb(CsBook *self, Link *link)
{
        //CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        g_debug("CS_BOOK >>> link selected: %s", link->uri);
        if (!g_ascii_strcasecmp(CHMSEE_NO_LINK, link->uri))
                return;

        CsIhtml *html = get_active_html(self);

        g_signal_handlers_block_by_func(html, html_uri_opened_cb, self);
        cs_ihtml_open_uri(html, link->uri); //FIXME: full uri?
        g_signal_handlers_unblock_by_func(html, html_uri_opened_cb, self);
}

static void
html_notebook_switch_page_cb(GtkNotebook *notebook, GtkNotebookPage *page, guint new_page_num, CsBook *self)
{
        g_debug("CS_BOOK >>> enter switch page callback page-num = %d", new_page_num);
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        GtkWidget *new_page = gtk_notebook_get_nth_page(notebook, new_page_num);
        g_debug("CS_BOOK >>> enter switch page callback, page = %p, new_page = %p", page, new_page);
        

        /* if (new_page) { */
        /*         CsIhtml* new_html = g_object_get_data(G_OBJECT (new_page), "html"); */

        /*         const gchar* title = cs_ihtml_get_title(new_html); //FIXME: if title == ""? */
        /*         const gchar* location = cs_ihtml_get_location(new_html); */

        /*         if (location != NULL && strlen(location)) { */
        /*                 Link *link = link_new(LINK_TYPE_PAGE, title, location); */
        /*                 cs_bookmarks_set_current_link(CS_BOOKMARKS (priv->bookmarks_page), link); */
        /*                 link_free(link); */
                
        /*                 /\* sync link to toc_page *\/ */
        /*                 if (priv->has_toc) */
        /*                         cs_toc_select_uri(CS_TOC (priv->toc_page), location); */
        /*         } */
        /* } */

        g_signal_emit(self, signals[HTML_CHANGED], 0, get_active_html(self));
}

static void
html_location_changed_cb(CsIhtml *html, const gchar *location, CsBook *self)
{
        g_debug("CS_BOOK >>> html location changed cb: %s", location);
        g_signal_emit(self, signals[HTML_CHANGED], 0, get_active_html(self));
}

static gboolean
html_uri_opened_cb(CsIhtml *html, const gchar *uri, CsBook *self)
{
        g_debug("CS_BOOK >>> enter html_uri_opened_cb with uri = %s", uri);
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
                        g_debug("%s:%d:html_uri_opened_cb:%s does not exist", __FILE__, __LINE__, uri+prefix_len);
                        gchar* newfname = correct_filename(uri+prefix_len);
                        if(newfname) {
                                g_debug(_("CS_BOOK >>> URI redirect: \"%s\" -> \"%s\""), uri, newfname);
                                cs_ihtml_open_uri(html, newfname);
                                g_free(newfname);
                                return TRUE;
                        }
                }
        }

        if ((html == get_active_html(self)) && priv->has_toc)
                cs_toc_select_uri(CS_TOC (priv->toc_page), uri);

        return FALSE;
}

static void
html_title_changed_cb(CsIhtml *html, const gchar *title, CsBook *self)
{
        g_debug("CSBOOK >>> html title changed cb %s", title);

        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        update_tab_title(self, html);

        /* sync bookmarks title entry */
        const gchar *location = cs_ihtml_get_location(html);

        if (location != NULL && strlen(location)) {
                Link *link = link_new(LINK_TYPE_PAGE, title, location);
                cs_bookmarks_set_current_link(CS_BOOKMARKS (priv->bookmarks_page), link);
                link_free(link);
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
html_context_link_cb(CsIhtml *html, const gchar *link, CsBook *self)
{
        g_debug("html context-link event: %s", link);

        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        set_context_menu_link(self, link);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "OpenLinkInNewTab"),
                                 g_str_has_prefix(priv->context_menu_link, "file://"));

        gtk_menu_popup(GTK_MENU(gtk_ui_manager_get_widget(priv->ui_manager, "/HtmlContextLink")),
                       NULL, NULL, NULL, NULL, 0, GDK_CURRENT_TIME);
}

static void
html_open_new_tab_cb(CsIhtml *html, const gchar *location, CsBook *self)
{
        g_debug("CS_BOOK >>> html open new tab callback: %s", location);

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
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        gint num_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK (priv->html_notebook));

        if (num_pages == 1) {
                /* TODO: should open a new empty tab */
                return;
        }

        gint number = -1, i;
        GtkWidget *tab_label, *page;

        for (i = 0; i < num_pages; i++) {
                GList *children, *l;

                g_debug("CS_BOOK >>> close tab find page %d", i);
                page = gtk_notebook_get_nth_page(GTK_NOTEBOOK (priv->html_notebook), i);

                tab_label = gtk_notebook_get_tab_label(GTK_NOTEBOOK (priv->html_notebook), page);
                children = gtk_container_get_children(GTK_CONTAINER (tab_label));

                for (l = children; l; l = l->next) {
                        if (widget == l->data) {
                                g_debug("CS_BOOK >>> close tab found tab on page %d", i);
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
        g_debug("CSBOOK >>> copy");

        g_return_if_fail(GTK_IS_NOTEBOOK (priv->html_notebook));

        cs_ihtml_copy_selection(get_active_html(self));
}

static void
on_copy_page_location(GtkWidget *widget, CsBook *cs)
{
        CsIhtml* html = get_active_html(cs);
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
        g_debug("CS_BOOK >>> select all");

        g_return_if_fail(GTK_IS_NOTEBOOK (priv->html_notebook));

        html = get_active_html(self);
        cs_ihtml_select_all(html);
}

static void
on_back(GtkWidget *widget, CsBook *self)
{
        cs_ihtml_go_back(get_active_html(self));
}

static void
on_forward(GtkWidget *widget, CsBook *self)
{
        cs_ihtml_go_forward(get_active_html(self));
}

static void
on_zoom_in(GtkWidget *widget, CsBook *self)
{
        CsIhtml* html = get_active_html(self);
        if(html != NULL) {
                cs_ihtml_increase_size(html);
        }
}

static void
on_context_new_tab(GtkWidget *widget, CsBook *self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        g_debug("CS_BOOK >>> On context open new tab: %s", priv->context_menu_link);

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

static void
on_finder_close(GtkWidget *widget, CsBook *self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        gtk_widget_hide(priv->findbar);
}

/* Internal functions */

static void
cs_book_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
        CsBook *self = CS_BOOK(object);
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        switch (property_id) {
        case PROP_SIDEPANE_VISIBLE:
                if (g_value_get_boolean(value)) {
                        gtk_widget_show(gtk_paned_get_child1(GTK_PANED (priv->hpaned)));
                } else {
                        gtk_widget_hide(gtk_paned_get_child1(GTK_PANED (priv->hpaned)));
                }
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
cs_book_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
        CsBook *self = CS_BOOK(object);
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        switch (property_id) {
        case PROP_SIDEPANE_VISIBLE:
                g_value_set_boolean(value, GTK_WIDGET_VISIBLE (gtk_paned_get_child1(GTK_PANED (priv->hpaned))));
                break;
        case PROP_LINK_MESSAGE:
                g_value_set_string(value, priv->link_message);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static GtkWidget *
new_html_page(CsBook *self)
{
        g_debug("CS_BOOK >>> new html page");

        /* only allow local link */
        /* if (uri != NULL && !g_str_has_prefix(uri, "file://")) { */
        /*         g_debug("CS_BOOK >>> new_tab only alow local link = %s", uri); */
        /*         return; //FIXME: display a blank page? */
        /* } */

        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        CsHtmlGecko *html = cs_html_new();
        GtkWidget *gecko = GTK_WIDGET (cs_ihtml_get_widget(CS_IHTML (html)));

        /* CsHtmlGecko *html = cs_html_gecko_new(); */
        /* GtkWidget *gecko = GTK_WIDGET (cs_html_gecko_get_widget(html)); */

        GtkWidget *page = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME (page), GTK_SHADOW_IN);
        gtk_container_set_border_width(GTK_CONTAINER (page), 2);
        gtk_container_add(GTK_CONTAINER (page), GTK_WIDGET (gecko));

        g_object_set_data(G_OBJECT (page), "html", html);
        g_debug("CS_BOOK >>> set object html = %p", html);

        g_signal_connect(G_OBJECT (html),
                         "title-changed",
                         G_CALLBACK (html_title_changed_cb),
                         self);
        g_signal_connect(G_OBJECT (html),
                         "uri-opened",
                         G_CALLBACK (html_uri_opened_cb),
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

        return page;

        /* if (uri != NULL || !strlen(uri)) { */
        /*         cs_ihtml_open_uri(CS_IHTML (html), uri); */
                
        /*         /\* synchronize link with toc *\/ */
        /*         if (priv->has_toc) */
        /*                 cs_toc_select_uri(CS_TOC (priv->toc_page), uri); */

        /*         g_signal_emit(self, signals[HTML_CHANGED], 0, html); //FIXME: send with empty page? */
        /* } else { */
        /*         g_debug("CS_BOOK >>> before clear page html = %p", html); */
        /*         cs_ihtml_clear(html); // FIXME: or duplicate current opened page */
        /*         g_debug("CS_BOOK >>> after clear page"); */
        /* } */

        /* if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->html_notebook)) > 1) { */
        /*         g_debug("CS_BOOK >>> new tab set current page"); */
                
        /*         gtk_notebook_set_current_page(GTK_NOTEBOOK (priv->html_notebook), page_num); */
        /* } */
}

static CsIhtml *
get_active_html(CsBook *self)
{
        g_debug("CS_BOOK >>> get active html");

        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        gint page_n = gtk_notebook_get_n_pages(GTK_NOTEBOOK (priv->html_notebook));
        g_debug("CS_BOOK >>> get active html page count = %d", page_n);
        gint page_num = gtk_notebook_get_current_page(GTK_NOTEBOOK (priv->html_notebook));

        if (page_num == -1){
                g_debug("CS_BOOK >>> active html page num = -1! ");
                return NULL;
        }

        GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK (priv->html_notebook), page_num);

        return g_object_get_data(G_OBJECT (page), "html");
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

static void
update_tab_title(CsBook *self, CsIhtml *html)
{
        GtkWidget *page;
        GtkWidget *widget, *label;
        gint num_pages, i;

        g_debug("CS_BOOK >>> update tab title");
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        gchar        *title = g_strdup(cs_ihtml_get_title(html));
        g_debug("CS_BOOK >>> update tab title = %s", title);

        if (title == NULL || title[0] == '\0')
                title = g_strdup(_("No Title"));

        num_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK (priv->html_notebook));

        for (i = 0; i < num_pages; i++) {
                page = gtk_notebook_get_nth_page(GTK_NOTEBOOK (priv->html_notebook), i);

                if (gtk_bin_get_child(GTK_BIN (page)) == cs_ihtml_get_widget (html)) {
                        widget = gtk_notebook_get_tab_label(GTK_NOTEBOOK (priv->html_notebook), page);

                        label = g_object_get_data(G_OBJECT (widget), "label");

                        if (label != NULL)
                                gtk_label_set_text(GTK_LABEL (label), title);

                        break;
                }
        }
        g_free(title);
}

static void
set_context_menu_link(CsBook *self, const gchar *link)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        g_free(priv->context_menu_link);
        priv->context_menu_link = g_strdup(link);
}

/* External functions*/

GtkWidget *
cs_book_new(void)
{
        g_debug("CS_BOOK >>> create");
        
        return GTK_WIDGET (g_object_new(CS_TYPE_BOOK, NULL));
}
                
void
cs_book_set_model(CsBook *self, CsChmfile *model)
{
        g_return_if_fail(IS_CS_CHMFILE (model));

        g_debug("CS_BOOK >>> set model, file = %p", model);

        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);

        /* close opened book */
        if (priv->model) {
                g_object_unref(priv->model);

                /* remove all page tab */
                gint i, num;

                num = gtk_notebook_get_n_pages(GTK_NOTEBOOK (priv->control_notebook));
                for (i = 0; i < num; i++) {
                        g_debug("CS_BOOK >>> close opened book remove control notebook page = %d", i);
                        gtk_notebook_remove_page(GTK_NOTEBOOK (priv->control_notebook), num);
                }

                num = gtk_notebook_get_n_pages(GTK_NOTEBOOK (priv->html_notebook));
                for (i = 0; i < num; i++) {
                        g_debug("CS_BOOK >>> close opened book remove html notebook page = %d", i);
                        gtk_notebook_remove_page(GTK_NOTEBOOK (priv->html_notebook), num);
                }
        }

        g_debug("CS_BOOK >>> display book");
        priv->model = g_object_ref(model);

        gint cur_page = 0;

        /* TOC */
        GNode *toc_tree = cs_chmfile_get_toc_tree(model);
        if (toc_tree != NULL) {
                priv->toc_page = GTK_WIDGET (cs_toc_new());
                cs_toc_set_model(CS_TOC(priv->toc_page), toc_tree);
                cur_page = gtk_notebook_append_page(GTK_NOTEBOOK (priv->control_notebook),
                                                    priv->toc_page,
                                                    gtk_label_new(_("Topics")));

                g_signal_connect_swapped(G_OBJECT (priv->toc_page),
                                         "link-selected",
                                         G_CALLBACK (link_selected_cb),
                                         self);
                priv->has_toc = TRUE;
        } else {
                g_debug("CS_BOOK >>> this chmfile dose not include toc");
                priv->has_toc = FALSE;
        }

        /* index */
        GList* index_list = cs_chmfile_get_index_list(model);
        if(index_list != NULL) {
                priv->index_page = GTK_WIDGET (cs_index_new());
                cs_index_set_model(CS_INDEX(priv->index_page), index_list);
                cur_page = gtk_notebook_append_page(GTK_NOTEBOOK (priv->control_notebook),
                                                    priv->index_page,
                                                    gtk_label_new(_("Index")));

                g_signal_connect_swapped(G_OBJECT (priv->index_page),
                                         "link-selected",
                                         G_CALLBACK (link_selected_cb),
                                         self);
        } else {
                g_debug("CS_BOOK >>> this chmfile dose not include index");
        }

        /* bookmarks */
        GList *bookmarks_list = cs_chmfile_get_bookmarks_list(model);
        priv->bookmarks_page = GTK_WIDGET (cs_bookmarks_new());
        cs_bookmarks_set_model(CS_BOOKMARKS(priv->bookmarks_page), bookmarks_list);
        cur_page = gtk_notebook_append_page(GTK_NOTEBOOK (priv->control_notebook),
                                            priv->bookmarks_page,
                                            gtk_label_new (_("Bookmarks")));

        g_signal_connect_swapped(G_OBJECT (priv->bookmarks_page),
                                 "link-selected",
                                 G_CALLBACK (link_selected_cb),
                                 self);

        gtk_notebook_set_current_page(GTK_NOTEBOOK (priv->control_notebook), cur_page);
        gtk_widget_show_all(priv->control_notebook);

        /* HTML notebook page */
        GtkWidget *page = new_html_page(self);

        g_debug("CS_BOOK >>> new html page = %p", page);

        /* customized label, add a close button rightmost */
        GtkWidget *tab_label = new_tab_label(self, _("No Title"));
        g_debug("CS_BOOK >>> new html 0 tab_label = %p", tab_label);

        gint pageNO= gtk_notebook_get_current_page(GTK_NOTEBOOK (priv->html_notebook));
        g_debug("CS_BOOK >>> new html 1 pageNO = %d", pageNO);
        
        pageNO = gtk_notebook_append_page(GTK_NOTEBOOK (priv->html_notebook), page, tab_label);
        g_debug("CS_BOOK >>> new html 2 pageNO = %d", pageNO);
        gtk_notebook_set_tab_label_packing(GTK_NOTEBOOK (priv->html_notebook),
                                           page,
                                           TRUE, TRUE,
                                           GTK_PACK_START);

        g_debug("CS_BOOK >>> new html 3");
        gtk_widget_show(page);
        g_debug("CS_BOOK >>> new html 4");
        gtk_widget_show_all(priv->html_notebook);
        gtk_notebook_set_current_page(GTK_NOTEBOOK (priv->html_notebook), 0);

        const gchar *homepage = cs_chmfile_get_homepage(model);
        if (homepage) {
                g_debug("CS_BOOK >>> open homepage = %s", homepage);
                /* g_debug("CS_BOOK >>> open homepage get html= %p", html); */
                gchar *uri = g_build_filename(cs_chmfile_get_bookfolder(model), homepage, NULL);
                g_debug("CS_BOOK >>> open homepage uri = %s", uri);

                CsHtmlGecko *html = g_object_get_data(G_OBJECT (page), "html");
                cs_ihtml_open_uri(CS_IHTML (html), uri);

                /* CsHtmlGecko *html = g_object_get_data(G_OBJECT (page), "html"); */
                /* cs_html_gecko_open_uri(html, uri); */

                /* if (priv->has_toc) */
                /*         cs_toc_select_uri(CS_TOC (priv->toc_page), uri); */

                /* g_free(uri); */

                /* const gchar *title = cs_ihtml_get_title(html); */
                /* g_debug("CS_BOOK >>> set label title title = %s", title); */
                /* update_tab_title(self, html); */
        }

        g_signal_connect(G_OBJECT (priv->html_notebook),
                         "switch-page",
                         G_CALLBACK (html_notebook_switch_page_cb),
                         self);

        g_signal_emit(self, signals[MODEL_CHANGED], 0, model);
}

void
cs_book_new_tab(CsBook *self, const gchar *uri)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        GtkWidget *page = new_html_page(self);

        /* customized label, add a close button rightmost */
        GtkWidget *tab_label = new_tab_label(self, _("No Title"));

        gtk_notebook_append_page(GTK_NOTEBOOK (priv->html_notebook), page, tab_label);

        if (strlen(uri)) {
                CsIhtml *html = g_object_get_data(G_OBJECT (page), "html");
                gchar *full_uri = g_build_filename(cs_chmfile_get_bookfolder(priv->model), uri, NULL);
                cs_ihtml_open_uri(CS_IHTML (html), full_uri);
                g_free(full_uri);

                update_tab_title(self, html);
        }

        gtk_widget_show(page);
}

void
cs_book_close_current_tab(CsBook *self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        g_return_if_fail(GTK_IS_NOTEBOOK (priv->html_notebook));

        if (gtk_notebook_get_n_pages(GTK_NOTEBOOK (priv->html_notebook)) == 1) {
                return; //FIXME: quit chmsee?
        }

        gint page_num = gtk_notebook_get_current_page(GTK_NOTEBOOK (priv->html_notebook));

        if (page_num >= 1) {
                gtk_notebook_remove_page(GTK_NOTEBOOK (priv->html_notebook), page_num);
        }
}

void
cs_book_reload_current_page(CsBook *self)
{
        g_debug("CS_BOOK >>> Reload current page");

        CsIhtml *html = get_active_html(self);
        const gchar *location = cs_ihtml_get_location(html);

        cs_ihtml_open_uri(html, location);
}

void
cs_book_homepage(CsBook *self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE(self);
        CsIhtml       *html = get_active_html(self);

        const gchar *homepage = cs_chmfile_get_homepage(priv->model);
        gchar *full_uri = g_build_filename(cs_chmfile_get_bookfolder(priv->model), homepage, NULL);
        g_debug("CS_BOOK >>> open homepage = %s", full_uri);
        /* g_signal_handlers_block_by_func(html, html_open_uri_cb, self); */ //FIXME: necessary?

        cs_ihtml_open_uri(html, full_uri);

        /* g_signal_handlers_unblock_by_func(html, html_open_uri_cb, self); */
        g_free(full_uri);

        if (priv->has_toc) {
                cs_toc_select_uri(CS_TOC (priv->toc_page), homepage);
        }
}

gboolean
cs_book_can_go_back(CsBook *self)
{
        return cs_ihtml_can_go_back(get_active_html(self));
}

gboolean
cs_book_can_go_forward(CsBook *self)
{
        return cs_ihtml_can_go_forward(get_active_html(self));
}

void
cs_book_go_back(CsBook *self)
{
        cs_ihtml_go_back(get_active_html(self));
}

void
cs_book_go_forward(CsBook *self)
{
        cs_ihtml_go_forward(get_active_html(self));
}

void
cs_book_zoom_in(CsBook *self)
{
        cs_ihtml_increase_size(get_active_html(self));
}

void
cs_book_zoom_out(CsBook *self)
{
        cs_ihtml_decrease_size(get_active_html(self));
}

void
cs_book_zoom_reset(CsBook *self)
{
        cs_ihtml_reset_size(get_active_html(self));
}

void
cs_book_copy(CsBook *self)
{
        cs_ihtml_copy_selection(get_active_html(self));
}

void
cs_book_select_all(CsBook *self)
{
        cs_ihtml_select_all(get_active_html(self));
}

const gchar *
cs_book_get_location(CsBook *self)
{
        return cs_ihtml_get_location(get_active_html(self));
}

int
cs_book_get_hpaned_position(CsBook *self)
{
        CsBookPrivate *priv = CS_BOOK_GET_PRIVATE (self);
        return gtk_paned_get_position(GTK_PANED (priv->hpaned));
}

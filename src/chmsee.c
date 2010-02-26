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

/***************************************************************************
 *   Copyright (C) 2003 by zhong                                           *
 *   zhongz@163.com                                                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <unistd.h>             /* R_OK */

#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include "chmsee.h"
#include "html-factory.h"
#include "setup.h"
#include "link.h"
#include "utils.h"
#include "componets/book.h"
#include "models/chmfile.h"

#define CHMSEE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CHMSEE_TYPE, ChmseePrivate))

static void chmsee_finalize(GObject *);
static void chmsee_dispose(GObject *);

static gboolean delete_cb(GtkWidget *, GdkEvent *, Chmsee *);
static void destroy_cb(GtkWidget *, Chmsee *);
static gboolean scroll_event_cb(Chmsee *, GdkEventScroll *);
static void map_cb(Chmsee *);
static gboolean window_state_event_cb(Chmsee *, GdkEventWindowState *);
static gboolean configure_event_cb(GtkWidget *, GdkEventConfigure *, Chmsee *);
static void book_model_changed_cb(Chmsee *, CsChmfile *);
static void book_html_changed_cb(Chmsee *, CsIhtml *);
static void book_html_link_message_notify_cb(Chmsee *, GParamSpec *, CsChmfile *);

static void open_file_response_cb(GtkWidget *, gint, Chmsee *);
static void about_response_cb(GtkDialog *, gint, gpointer);

static void on_open_file(GtkWidget *, Chmsee *);
static void on_open_new_tab(GtkWidget *, Chmsee *);
static void on_context_new_tab(GtkWidget *, Chmsee *);
static void on_close_current_tab(GtkWidget *, Chmsee *);

static void on_home(GtkWidget *, Chmsee *);
static void on_back(GtkWidget *, Chmsee *);
static void on_forward(GtkWidget *, Chmsee *);
static void on_zoom_in(GtkWidget *, Chmsee *);
static void on_zoom_reset(GtkWidget *, Chmsee *);
static void on_zoom_out(GtkWidget *, Chmsee *);
static void on_setup(GtkWidget *, Chmsee *);
static void on_about(GtkWidget *);
static void on_copy(GtkWidget *, Chmsee *);
static void on_copy_page_location(GtkWidget*, Chmsee*);
static void on_select_all(GtkWidget *, Chmsee *);
static void on_context_copy_link(GtkWidget *, Chmsee *);
static void on_keyboard_escape(GtkWidget *, Chmsee *);
static void on_fullscreen_toggled(GtkWidget *, Chmsee *);
static void on_sidepane_toggled(GtkWidget *, Chmsee *);

static GtkWidget *get_widget(Chmsee *, gchar *);
static void chmsee_quit(Chmsee *);
static void populate_windows(Chmsee *);
static void chmsee_set_model(Chmsee *, CsChmfile *);
static void chmsee_set_fullscreen(Chmsee *, gboolean);
static void show_sidepane(Chmsee *);
static void hide_sidepane(Chmsee *);
static void set_sidepane_state(Chmsee *, gboolean);
static void chmsee_open_draged_file(Chmsee *, const gchar *);
static void chmsee_drag_data_received(GtkWidget *, GdkDragContext *, gint, gint,
                                      GtkSelectionData *, guint, guint);
static void update_status_bar(Chmsee *, const gchar *);

typedef struct _ChmseePrivate ChmseePrivate;

struct _ChmseePrivate {
        GtkWidget       *menubar;
        GtkWidget       *toolbar;
        GtkWidget       *book;
        Gtkwidget       *findbar;
        GtkWidget       *statusbar;

        CsChmfile       *chmfile;
        CsConfig        *config;

        GtkActionGroup  *action_group;
        GtkUIManager    *ui_manager;
        guint            scid_default;

        gboolean         expect_fullscreen;
        gchar           *context_menu_link;
        gint             state; /* see enum CHMSEE_STATE_* */
};

enum {
        CHMSEE_STATE_INIT,    /* init state, no book is loaded */
        CHMSEE_STATE_LOADING, /* loading state, don't pop up an error window when open homepage failed */
        CHMSEE_STATE_NORMAL   /* normal state, one book is loaded */
};

/* static gchar *context_menu_link = NULL; */
static const GtkTargetEntry view_drop_targets[] = {
        { "text/uri-list", 0, 0 }
};

/* Normal items */
static const GtkActionEntry entries[] = {
        { "FileMenu", NULL, "_File" },
        { "EditMenu", NULL, "_Edit" },
        { "ViewMenu", NULL, "_View" },
        { "HelpMenu", NULL, "_Help" },

        { "Open", GTK_STOCK_OPEN, "_Open", "<control>O", "Open a file", G_CALLBACK(on_open_file)},
        { "NewTab", NULL, "_New Tab", "<control>T", NULL, G_CALLBACK(on_open_new_tab)},
        { "CloseTab", NULL, "_Close Tab", "<control>W", NULL, G_CALLBACK(on_close_current_tab)},
        { "Exit", GTK_STOCK_QUIT, "E_xit", "<control>Q", "Exit the program", G_CALLBACK(destroy_cb)},

        { "Copy", GTK_STOCK_COPY, "_Copy", "<control>C", NULL, G_CALLBACK(on_copy)},
        { "Preferences", GTK_STOCK_PREFERENCES, "_Preferences", NULL, NULL, G_CALLBACK(on_setup)},

        { "Home", GTK_STOCK_HOME, "_Home", NULL, NULL, G_CALLBACK(on_home)},
        { "Back", GTK_STOCK_GO_BACK, "_Back", "<alt>Left", NULL, G_CALLBACK(on_back)},
        { "Forward", GTK_STOCK_GO_FORWARD, "_Forward", "<alt>Right", NULL, G_CALLBACK(on_forward)},

        { "About", GTK_STOCK_ABOUT, "_About", NULL, NULL, G_CALLBACK(on_about)},

        { "ZoomIn", GTK_STOCK_ZOOM_IN, "Zoom _In", "<control>plus", NULL, G_CALLBACK(on_zoom_in)},
        { "ZoomReset", GTK_STOCK_ZOOM_100, "Normal Size", "<control>0", NULL, G_CALLBACK(on_zoom_reset)},
        { "ZoomOut", GTK_STOCK_ZOOM_OUT, "Zoom _Out", "<control>minus", NULL, G_CALLBACK(on_zoom_out)},

        { "OpenLinkInNewTab", NULL, "Open Link in New _Tab", NULL, NULL, G_CALLBACK(on_context_new_tab)},
        { "CopyLinkLocation", NULL, "_Copy Link Location", NULL, NULL, G_CALLBACK(on_context_copy_link)},
        { "SelectAll", NULL, "Select _All", NULL, NULL, G_CALLBACK(on_select_all)},
        { "CopyPageLocation", NULL, "Copy Page _Location", NULL, NULL, G_CALLBACK(on_copy_page_location)},

        { "OnKeyboardEscape", NULL, NULL, "Escape", NULL, G_CALLBACK(on_keyboard_escape)},
        { "OnKeyboardControlEqual", NULL, NULL, "<control>equal", NULL, G_CALLBACK(on_zoom_in)}
};

/* Toggle items */
static const GtkToggleActionEntry toggle_entries[] = {
        { "FullScreen", NULL, "_Full Screen", "F11", "Switch between full screen and windowed mode", G_CALLBACK(on_fullscreen_toggled), FALSE },
        { "SidePane", NULL, "Side _Pane", "F9", NULL, G_CALLBACK(on_sidepane_toggled), TRUE }
};

/* Radio items */
static const GtkRadioActionEntry radio_entries[] = {
};

static const char *ui_description =
        "<ui>"
        "  <menubar name='MainMenu'>"
        "    <menu action='FileMenu'>"
        "      <menuitem action='Open'/>"
        "      <menuitem action='NewTab'/>"
        "      <menuitem action='CloseTab'/>"
        "      <separator/>"
        "      <menuitem action='Exit'/>"
        "    </menu>"
        "    <menu action='EditMenu'>"
        "      <menuitem action='SelectAll'/>"
        "      <menuitem action='Copy'/>"
        "      <separator/>"
        "      <menuitem action='Preferences'/>"
        "    </menu>"
        "    <menu action='ViewMenu'>"
        "      <menuitem action='FullScreen'/>"
        "      <menuitem action='SidePane'/>"
        "      <separator/>"
        "      <menuitem action='Home'/>"
        "      <menuitem action='Back'/>"
        "      <menuitem action='Forward'/>"
        "      <separator/>"
        "      <menuitem action='ZoomIn'/>"
        "      <menuitem action='ZoomReset'/>"
        "      <menuitem action='ZoomOut'/>"
        "    </menu>"
        "    <menu action='HelpMenu'>"
        "      <menuitem action='About'/>"
        "    </menu>"
        "  </menubar>"
        "  <toolbar name='toolbar'>"
        "    <toolitem action='Open'/>"
        "    <separator/>"
        "    <toolitem action='SidePane' name='sidepane'/>"
        "    <toolitem action='Back'/>"
        "    <toolitem action='Forward'/>"
        "    <toolitem action='Home'/>"
        "    <toolitem action='ZoomIn'/>"
        "    <toolitem action='ZoomReset'/>"
        "    <toolitem action='ZoomOut'/>"
        "    <toolitem action='Preferences'/>"
        "    <toolitem action='About'/>"
        "  </toolbar>"
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
        "  <accelerator action='OnKeyboardEscape'/>"
        "  <accelerator action='OnKeyboardControlEqual'/>"
        "</ui>";

/* GObject functions */

G_DEFINE_TYPE (Chmsee, chmsee, GTK_TYPE_WINDOW);

static void
chmsee_class_init(ChmseeClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        g_type_class_add_private(klass, sizeof(ChmseePrivate));

        object_class->finalize = chmsee_finalize;
        object_class->dispose  = chmsee_dispose;

        GTK_WIDGET_CLASS(klass)->drag_data_received = chmsee_drag_data_received;
}

static void
chmsee_init(Chmsee* self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        priv->chmfile = NULL;
        priv->context_menu_link = NULL;
        priv->expect_fullscreen = FALSE;
        priv->state = CHMSEE_STATE_INIT;

        gtk_widget_add_events(GTK_WIDGET(self),
                              GDK_STRUCTURE_MASK | GDK_BUTTON_PRESS_MASK );

        g_signal_connect(G_OBJECT (self),
                         "scroll-event",
                         G_CALLBACK (scroll_event_cb),
                         NULL);
        g_signal_connect(G_OBJECT (self),
                         "map",
                         G_CALLBACK (map_cb),
                         NULL);
        g_signal_connect(G_OBJECT (self),
                         "window-state-event",
                         G_CALLBACK (window_state_event_cb),
                         NULL);

        gtk_drag_dest_set(GTK_WIDGET (self),
                          GTK_DEST_DEFAULT_ALL,
                          view_drop_targets,
                          G_N_ELEMENTS (view_drop_targets),
                          GDK_ACTION_COPY);

        /* quit event handle */
        g_signal_connect(G_OBJECT (self),
                         "delete_event",
                         G_CALLBACK (delete_cb),
                         self);
        g_signal_connect(G_OBJECT (self),
                         "destroy",
                         G_CALLBACK (destroy_cb),
                         self);

        /* widget size changed event handle */
        g_signal_connect(G_OBJECT (self),
                         "configure-event",
                         G_CALLBACK (configure_event_cb),
                         self);

        /* startup html render engine */
        if(!cs_html_init_system()) {
                g_error("Initialize html render engine failed!");
                exit(1);
        }

        populate_windows(self);
}

static void
chmsee_dispose(GObject *gobject)
{
        Chmsee        *self = CHMSEE(gobject);
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        GtkBuilder *builder = g_object_get_data(G_OBJECT (self), "builder");

        g_object_unref(G_OBJECT (builder));

        g_object_unref(priv->chmfile);
        g_object_unref(priv->action_group);
        g_object_unref(priv->ui_manager);

        G_OBJECT_CLASS(chmsee_parent_class)->dispose(gobject);
}

static void
chmsee_finalize(GObject *object)
{
        Chmsee        *self = CHMSEE(object);
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        g_free(priv->context_menu_link);
        //FIXME: call chmsee_quit?
        G_OBJECT_CLASS (chmsee_parent_class)->finalize (object);
}

/* Callbacks */

static gboolean
delete_cb(GtkWidget *widget, GdkEvent *event, Chmsee *self)
{
        g_message("window delete");
        return FALSE;
}

static void
destroy_cb(GtkWidget *widget, Chmsee *self)
{
        chmsee_quit(self);
}

static gboolean
scroll_event_cb(Chmsee *self, GdkEventScroll *event)
{
        if(event->direction == GDK_SCROLL_UP && (event->state & GDK_CONTROL_MASK)) {
                on_zoom_in(NULL, self);
                return TRUE;
        } else if(event->direction == GDK_SCROLL_DOWN && (event->state & GDK_CONTROL_MASK)) {
                on_zoom_out(NULL, self);
                return TRUE;
        } else {
                g_debug("scrollevent->direction: %d", event->direction);
                g_debug("scrollevent->state: %x", event->state);
        }

        return FALSE;
}

static void
map_cb(Chmsee* self)
{
        if (priv->hpaned_position >= 0) {
                g_object_set(G_OBJECT(priv->book),
                             "position", priv->hpaned_position,
                             NULL
                        );
        }
}

static gboolean
window_state_event_cb(Chmsee *self, GdkEventWindowState *event)
{
        g_return_val_if_fail(IS_CHMSEE(self), FALSE);
        g_return_val_if_fail(event->type == GDK_WINDOW_STATE, FALSE);

        g_debug("enter on_window_state_event with event->changed_mask = %d and event->new_window_state = %d",
                event->changed_mask,
                event->new_window_state
                );

        if(!(event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)) {
                return FALSE;
        }

        if(event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) {
                if(priv->expect_fullscreen) {
                        on_fullscreen(self);
                } else {
                        g_warning("expect not fullscreen but got a fullscreen event, restored");
                        chmsee_set_fullscreen(self, FALSE);
                        return TRUE;
                }
        } else {
                if(!priv->expect_fullscreen) {
                        on_unfullscreen(self);
                } else {
                        g_warning("expect fullscreen but got an unfullscreen event, restored");
                        chmsee_set_fullscreen(self, TRUE);
                        return TRUE;
                }
        }

        return FALSE;
}

static gboolean
configure_event_cb(GtkWidget *widget, GdkEventConfigure *event, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        if (event->width != priv->config->width || event->height != priv->config->height) {
                cs_book_reload_current_page(self);
        }

        if(!priv->config->fullscreen) {
                priv->config->width  = event->width;
                priv->config->height = event->height;
                priv->config->pos_x  = event->x;
                priv->config->pos_y  = event->y;
        }

        return FALSE;
}

static void
book_model_changed_cb(Chmsee *self, CsChmfile *chmfile)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        gboolean has_model = (chmfile != NULL);

        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "NewTab"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "CloseTab"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "Home"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "SidePane"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "ZoomIn"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "ZoomOut"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "ZoomReset"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "Back"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "Forward"), has_model);

        gtk_widget_set_sensitive(priv->book, has_model);
}

static void
book_html_changed_cb(Chmsee *self, CsIhtml *html)
{
        gboolean back_state, forward_state;
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        g_debug("%s:%d:recieve html_changed signal from %p", __FILE__, __LINE__, html);

        back_state = cs_book_can_go_back(priv->book);
        forward_state = cs_book_can_go_forward(priv->book);

        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "Back"), back_state);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "Forward"), forward_state);
}

static void
book_html_link_message_notify_cb(Chmsee *self, GParamSpec *pspec, CsChmfile *chmfile)
{
        gchar* link_message;
        g_object_get(chmfile,
                     "link-message", &link_message,
                     NULL);

        update_status_bar(self, link_message);
        g_free(link_message);
}

static void
open_file_response_cb(GtkWidget *widget, gint response_id, Chmsee *chmsee)
{
        gchar *filename = NULL;

        if (response_id == GTK_RESPONSE_OK)
                filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (widget));

        gtk_widget_destroy(widget);

        if (filename != NULL)
                chmsee_open_file(chmsee, filename);

        g_free(filename);
}

#if 0
/* Popup html context menu */
static void
html_context_normal_cb(CsIhtml *html, Chmsee *self)
{
        g_message("html context-normal event");
        gtk_menu_popup(GTK_MENU(gtk_ui_manager_get_widget(priv->ui_manager, "/HtmlContextNormal")),
                       NULL, NULL, NULL, NULL, 0, GDK_CURRENT_TIME);
}

/* Popup html context menu when mouse over hyper link */
static void
html_context_link_cb(CsIhtml *html, const gchar *link, Chmsee* self)
{
        g_debug("html context-link event: %s", link);
        chmsee_set_context_menu_link(self, link);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "OpenLinkInNewTab"),
                                 g_str_has_prefix(priv->context_menu_link, "file://"));

        gtk_menu_popup(GTK_MENU(gtk_ui_manager_get_widget(priv->ui_manager, "/HtmlContextLink")),
                       NULL, NULL, NULL, NULL, 0, GDK_CURRENT_TIME);

}
#endif

/* Toolbar button events */

static void
on_open_file(GtkWidget *widget, Chmsee *self)
{
        GtkBuilder    *builder;
        GtkWidget     *dialog;
        GtkFileFilter *filter;

        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        /* create open file dialog */
        builder = gtk_builder_new();
        gtk_builder_add_from_file(builder, get_resource_path("openfile-dialog.ui"), NULL);

        dialog = GTK_WIDGET (gtk_builder_get_object(builder, "openfile_dialog"));

        g_signal_connect(G_OBJECT (dialog),
                         "response",
                         G_CALLBACK (open_file_response_cb),
                         self);

        /* file list fiter */
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, _("CHM Files"));
        gtk_file_filter_add_pattern(filter, "*.[cC][hH][mM]");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER (dialog), filter);

        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, _("All Files"));
        gtk_file_filter_add_pattern(filter, "*");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER (dialog), filter);

        /* previous opened file folder */
        if (priv->config->last_dir) {
                gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (dialog), priv->config->last_dir);
        }

        g_object_unref(G_OBJECT (builder));
}

static void
on_copy(GtkWidget *widget, Chmsee *self)
{
        g_debug("Chmsee: On Copy");
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_book_copy(priv->book);
}

static void
on_copy_page_location(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        const gchar* location = cs_book_get_location(priv->book); // FIXME: free location?

        if(!location) return;

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
on_select_all(GtkWidget *widget, Chmsee *self)
{
        g_debug("Chmsee: On Select All");

        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_book_select_all(priv->book);
}

static void
on_setup(GtkWidget *widget, Chmsee *self)
{
        setup_window_new(self);
}

static void
on_back(GtkWidget *widget, Chmsee *chmsee)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_book_go_back(priv->book);
}

static void
on_forward(GtkWidget *widget, Chmsee *chmsee)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_book_go_forward(priv->book);
}

static void
on_home(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_book_homepage(priv->book);
}

static void
on_zoom_in(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_book_zoom_in(priv->book);
}

static void
on_zoom_reset(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_book_zoom_reset(priv->book);
}

static void
on_zoom_out(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_book_zoom_out(priv->book);
}

static void
about_response_cb(GtkDialog *dialog, gint response_id, gpointer user_data)
{
        if (response_id == GTK_RESPONSE_CANCEL)
                gtk_widget_destroy(GTK_WIDGET (dialog));
}

static void
on_about(GtkWidget *widget)
{

        GtkBuilder *builder = gtk_builder_new();
        gtk_builder_add_from_file(builder, get_resource_path("about-dialog.ui"), NULL);

        GtkWidget *dialog = GTK_WIDGET (gtk_builder_get_object(builder, "about_dialog"));

        g_signal_connect(G_OBJECT (dialog),
                         "response",
                         G_CALLBACK (about_response_cb),
                         NULL);

        gtk_about_dialog_set_version(GTK_ABOUT_DIALOG (dialog), PACKAGE_VERSION);

        g_object_unref(builder);
}

static void
on_open_new_tab(GtkWidget *widget, Chmsee *self)
{
        g_debug("Chmsee: Open new tab");

        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_book_create_new_tab(priv->book);
}

static void
on_context_new_tab(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        g_debug("Chmsee: On context open new tab: %s", priv->context_menu_link);

        if (priv->context_menu_link != NULL) {
                cs_book_open_new_tab(priv->book, priv->context_menu_link);
        }
}

static void
on_close_current_tab(GtkWidget *widget, Chmsee *self)
{
        cs_book_closee_current_tab(priv->book);
}

static void
on_context_copy_link(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        g_debug("On context copy link: %s", priv->context_menu_link);

        if (priv->context_menu_link != NULL) {
                gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY),
                                       priv->context_menu_link, -1);
                gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
                                       priv->context_menu_link, -1);
        }
}

static void
on_keyboard_escape(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        if (priv->config->fullscreen) {
                chmsee_set_fullscreen(self, FALSE);
        } else {
                gtk_window_iconify(GTK_WINDOW (self));
        }
}

static void
on_fullscreen_toggled(GtkWidget *menu, Chmsee *self)
{
        g_return_if_fail(IS_CHMSEE(self));

        gboolean active;
        g_object_get(G_OBJECT (menu),
                     "active", &active,
                     NULL);
        g_debug("enter on_fullscreen_toggled with menu.active = %d", active);
        chmsee_set_fullscreen(self, active);
}

static void
on_sidepane_toggled(GtkWidget *menu, Chmsee *self)
{
        g_return_if_fail(IS_CHMSEE(self));
        gboolean active;
        g_object_get(G_OBJECT(menu),
                     "active", &active,
                     NULL);
        if(active) {
                show_sidepane(self);
        } else {
                hide_sidepane(self);
        }
}

/* internal functions */

static void
chmsee_quit(Chmsee *self)
{
        cs_ihtml_shutdown();

        gtk_main_quit();
}

static void
chmsee_drag_data_received (GtkWidget          *widget,
                           GdkDragContext     *context,
                           gint                x,
                           gint                y,
                           GtkSelectionData   *selection_data,
                           guint               info,
                           guint               time)
{
        gchar  **uris;
        gint     i = 0;

        uris = gtk_selection_data_get_uris (selection_data);
        if (!uris) {
                gtk_drag_finish (context, FALSE, FALSE, time);
                return;
        }

        for (i = 0; uris[i]; i++) {
                gchar* uri = uris[i];
                if(g_str_has_prefix(uri, "file://")
                   && (g_str_has_suffix(uri, ".chm")
                       || g_str_has_suffix(uri, ".CHM"))) {
                        chmsee_open_draged_file(CHMSEE(widget), uri);
                        break;
                }
        }

        gtk_drag_finish (context, TRUE, FALSE, time);

        g_strfreev (uris);
}

static void
chmsee_open_draged_file(Chmsee *chmsee, const gchar *file)
{
        if(!g_str_has_prefix(uri, "file://")) {
                return;
        }

        gchar* fname = g_uri_unescape_string(uri+7, NULL);
        chmsee_open_file(chmsee, fname);
        g_free(fname);
}

static GtkWidget *
get_widget(Chmsee *self, gchar *widget_name)
{
        GtkBuilder *builder = g_object_get_data(G_OBJECT (self), "builder");
        GtkWidget *widget = GTK_WIDGET (gtk_builder_get_object(builder, widget_name));

        return widget;
}

static void
populate_windows(Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        GtkBuilder *builder = gtk_builder_new();

        guint rv = gtk_builder_add_from_file(builder, get_resource_path("main-box.ui"), NULL);

        if (rv == 0) {
                g_error("Cannot find main-box.ui GtkBuilder file!");
                exit(1);
        }

        g_object_set_data(G_OBJECT (self), "builder", builder);

        GtkWidget *vbox = GTK_WIDGET (gtk_builder_get_object(builder, "main_vbox"));

        GtkActionGroup* action_group = gtk_action_group_new("MenuActions");
        priv->action_group = action_group;
        gtk_action_group_add_actions(action_group, entries, G_N_ELEMENTS (entries), self);
        gtk_action_group_add_toggle_actions(action_group, toggle_entries, G_N_ELEMENTS (toggle_entries), self);

        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "NewTab"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "CloseTab"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "Home"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "Back"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "Forward"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "SidePane"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "ZoomIn"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "ZoomOut"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "ZoomReset"), FALSE);

        GtkUIManager* ui_manager = gtk_ui_manager_new();
        priv->ui_manager = ui_manager;
        gtk_ui_manager_insert_action_group(ui_manager, action_group, 0);

        GtkAccelGroup* accel_group = gtk_ui_manager_get_accel_group(ui_manager);
        gtk_window_add_accel_group(GTK_WINDOW (self), accel_group);

        GError* error = NULL;
        if (!gtk_ui_manager_add_ui_from_string(ui_manager, ui_description, -1, &error))
        {
                g_message ("building menus failed: %s", error->message);
                g_error_free (error);
                exit (EXIT_FAILURE);
        }

        GtkWidget* menubar = gtk_handle_box_new();
        priv->menubar = menubar;
        gtk_container_add(GTK_CONTAINER(menubar), gtk_ui_manager_get_widget (ui_manager, "/MainMenu"));
        gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, FALSE, 0);

        GtkWidget* toolbar = gtk_handle_box_new();
        priv->toolbar = toolbar;
        gtk_container_add(GTK_CONTAINER(toolbar), gtk_ui_manager_get_widget(ui_manager, "/toolbar"));
        gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

        GtkWidget* book = cs_book_new();
        priv->book = book;

        gtk_box_pack_start(GTK_BOX(vbox), book, TRUE, TRUE, 0);
        gtk_container_set_focus_child(GTK_CONTAINER(vbox), book);
        gtk_widget_set_sensitive(book, FALSE);

        g_signal_connect_swapped(book,
                                 "model_changed",
                                 G_CALLBACK (book_model_changed_cb),
                                 self);
        g_signal_connect_swapped(book,
                                 "html_changed",
                                 G_CALLBACK (book_html_changed_cb),
                                 self);
        g_signal_connect_swapped(book,
                                 "notify::link-message",
                                 G_CALLBACK (book_html_link_message_notify_cb),
                                 self);

        gtk_tool_button_set_icon_widget(
                GTK_TOOL_BUTTON(gtk_ui_manager_get_widget(ui_manager, "/toolbar/sidepane")),
                gtk_image_new_from_file(get_resource_path("show-pane.png")));

        /* find bar */
        priv->findbar = GTK_WIDGET (gtk_builder_get_object(builder, "find_hbox"));

        /* status bar */
        priv->statusbar = GTK_WIDGET (gtk_builder_get_object(builder, "statusbar"));
        priv->scid_default = gtk_statusbar_get_context_id(GTK_STATUSBAR (priv->statusbar), "default");

        gtk_container_add(GTK_CONTAINER (self), vbox);
        gtk_widget_show_all(self);

        accel_group = g_object_new(GTK_TYPE_ACCEL_GROUP, NULL);
        gtk_window_add_accel_group(GTK_WINDOW (self), accel_group);
        
        update_status_bar(self, _("Ready!"));
}

static void
chmsee_set_fullscreen(Chmsee *self, gboolean fullscreen)
{
        g_debug("enter chmsee_set_fullscreen with fullscreen = %d", fullscreen);

        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        priv->expect_fullscreen = fullscreen;

        if(fullscreen) {
                g_debug("call gtk_window_fullscreen");
                gtk_window_fullscreen(GTK_WINDOW(self));
        } else {
                g_debug("call gtk_window_unfullscreen");
                gtk_window_unfullscreen(GTK_WINDOW(self));
        }
}

static void
update_status_bar(Chmsee *self, const gchar *message)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        gchar *status = g_strdup_printf(" %s", message);

        gtk_statusbar_pop(GTK_STATUSBAR(priv->statusbar), priv->scid_default);
        gtk_statusbar_push(GTK_STATUSBAR(priv->statusbar), priv->scid_default, status);

        g_free(status);
}

static void
set_sidepane_state(Chmsee* self, gboolean state)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        GtkWidget* icon_widget;
        g_object_set(priv->book,
                     "sidepane-visible", state,
                     NULL);

        if (state) {
                icon_widget = gtk_image_new_from_file(get_resource_path("hide-pane.png"));
        } else {
                icon_widget = gtk_image_new_from_file(get_resource_path("show-pane.png"));
        }
        gtk_widget_show(icon_widget);
        gtk_tool_button_set_icon_widget(
                GTK_TOOL_BUTTON(gtk_ui_manager_get_widget(priv->ui_manager, "/toolbar/sidepane")),
                icon_widget);
};

static void
show_sidepane(Chmsee *self)
{
        set_sidepane_state(self, TRUE);
}

static void
hide_sidepane(Chmsee *self)
{
        set_sidepane_state(self, FALSE);
}

static void
on_fullscreen(Chmsee *self)
{
        g_debug("enter on_fullscreen");
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        priv->config->fullscreen = TRUE;
        gtk_widget_hide(priv->menubar);
        gtk_widget_hide(priv->toolbar);
        gtk_widget_hide(get_widget(self, "statusbar"));
}

static void
on_unfullscreen(Chmsee *self)
{
        g_debug("enter on_unfullscreen");
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        priv->config->fullscreen = FALSE;
        gtk_widget_show(priv->menubar);
        gtk_widget_show(priv->toolbar);
        gtk_widget_show(get_widget(self, "statusbar"));
}

/* External functions */

Chmsee *
chmsee_new(CsConfig *config)
{
        Chmsee        *self = g_object_new(CHMSEE_TYPE, NULL);
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        priv->config = config;

        cs_html_set_default_lang(config->lang);

        if (config->pos_x >= 0 && config->pos_y >= 0)
                gtk_window_move(GTK_WINDOW (self), config->pos_x, config->pos_y);

        if (config->width > 0 && config->height > 0)
                gtk_window_resize(GTK_WINDOW (self), config->width, config->height);
        else
                gtk_window_resize(GTK_WINDOW (self), 800, 600);

        gtk_window_set_title(GTK_WINDOW (self), "ChmSee");
        gtk_window_set_icon_from_file(GTK_WINDOW (self), get_resource_path("chmsee-icon.png"), NULL);

        return self;
}

void
chmsee_open_file(Chmsee *self, const gchar *filename)
{
        g_return_if_fail(IS_CHMSEE (self));

        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        /* Close currently opened book */
        if (priv->chmfile) {
                g_object_unref(priv->chmfile);
        }

        /* Extract chmfile, get file infomation */
        priv->chmfile = cs_chmfile_new(filename, priv->config->bookshelf);

        if (priv->chmfile) {
                g_debug("Chmsee: display book");
                priv->state = CHMSEE_STATE_LOADING;

                cs_book_set_model(CS_BOOK(priv->book), priv->chmfile);

                /* update window title */
                gchar *bookname = cs_chmfile_get_bookname(priv->chmfile);
                gchar *window_title = g_strdup_printf("%s - ChmSee", bookname);
                gtk_window_set_title(GTK_WINDOW (self), window_title);
                g_free(window_title);

                if (priv->config->last_dir)
                        g_free(priv->config->last_dir);
                priv->config->last_dir = g_strdup(g_path_get_dirname(cs_chmfile_get_filename(priv->chmfile)));

                priv->state = CHMSEE_STATE_NORMAL;
        } else {
                /* Popup an error message dialog */
                GtkWidget *msg_dialog;

                msg_dialog = gtk_message_dialog_new(GTK_WINDOW (self),
                                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    GTK_MESSAGE_ERROR,
                                                    GTK_BUTTONS_CLOSE,
                                                    _("Error: Can not open spectified file '%s'"),
                                                    filename);
                gtk_dialog_run(GTK_DIALOG (msg_dialog));
                gtk_widget_destroy(msg_dialog);
        }
}

int chmsee_get_hpaned_position(Chmsee* self) {
        gint position;
        g_object_get(G_OBJECT(priv->ui_chmfile),
                     "position", &position,
                     NULL
                );
        return position;
}

void chmsee_set_hpaned_position(Chmsee* self, int hpaned_position) {
        priv->hpaned_position = hpaned_position;
        /*
          g_object_set(G_OBJECT(get_widget(self, "hpaned1")),
          "position", hpaned_position,
          NULL
          );
        */
}


const gchar* chmsee_get_cache_dir(Chmsee* self) {
        return priv->cache_dir;
}

const gchar* chmsee_get_variable_font(Chmsee* self) {
        g_return_val_if_fail(priv->book, NULL);
        return chmsee_ichmfile_get_variable_font(priv->book);
}

void chmsee_set_variable_font(Chmsee* self, const gchar* font_name) {
        g_return_if_fail(priv->book);
        chmsee_ichmfile_set_variable_font(priv->book, font_name);
}

const gchar* chmsee_get_fixed_font(Chmsee* self) {
        g_return_val_if_fail(priv->book, NULL);
        return chmsee_ichmfile_get_fixed_font(priv->book);
}

void chmsee_set_fixed_font(Chmsee* self, const gchar* font_name) {
        g_return_if_fail(priv->book);
        chmsee_ichmfile_set_fixed_font(priv->book, font_name);
}

int chmsee_get_lang(Chmsee* self) {
        return priv->lang;
}

void chmsee_set_lang(Chmsee* self, int lang) {
        priv->lang = lang;
}

gboolean chmsee_has_book(Chmsee* self) {
        return priv->book != NULL;
}

void chmsee_close_book(Chmsee *self) {
        if (priv->book) {
                g_object_unref(priv->book);
        }

        chmsee_ui_chmfile_set_model(CHMSEE_UI_CHMFILE(priv->ui_chmfile), NULL);

        gtk_window_set_title(GTK_WINDOW (self), "Chmsee");
        priv->state = CHMSEE_STATE_NORMAL;
}


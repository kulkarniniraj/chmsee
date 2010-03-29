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

#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include "chmsee.h"
#include "setup.h"
#include "link.h"
#include "utils.h"
#include "components/book.h"
#include "components/html-gecko.h"
#include "models/chmfile.h"

typedef struct _ChmseePrivate ChmseePrivate;

struct _ChmseePrivate {
        GtkWidget       *menubar;
        GtkWidget       *toolbar;
        GtkWidget       *book;
        GtkWidget       *statusbar;

        CsChmfile       *chmfile;
        CsConfig        *config;

        GtkActionGroup  *action_group;
        GtkUIManager    *ui_manager;
        guint            scid_default;

        gint             state; /* see enum CHMSEE_STATE_* */
};

enum {
        CHMSEE_STATE_INIT,    /* init state, no book is loaded */
        CHMSEE_STATE_LOADING, /* loading state, don't pop up an error window when open homepage failed */
        CHMSEE_STATE_NORMAL   /* normal state, one book is loaded */
};

static const GtkTargetEntry view_drop_targets[] = {
        { "text/uri-list", 0, 0 }
};

static const char *ui_description =
        "<ui>"
        "  <menubar name='MainMenu'>"
        "    <menu action='FileMenu'>"
        "      <menuitem action='Open'/>"
        "      <menuitem action='RecentFiles'/>"
        "      <separator/>"
        "      <menuitem action='NewTab'/>"
        "      <menuitem action='CloseTab'/>"
        "      <separator/>"
        "      <menuitem action='Exit'/>"
        "    </menu>"
        "    <menu action='EditMenu'>"
        "      <menuitem action='Copy'/>"
        "      <menuitem action='SelectAll'/>"
        "      <separator/>"
        "      <menuitem action='Find'/>"
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
        "      <separator/>"
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
        "  <accelerator action='OnKeyboardEscape'/>"
        "  <accelerator action='OnKeyboardControlEqual'/>"
        "</ui>";

#define CHMSEE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CHMSEE_TYPE, ChmseePrivate))

static void chmsee_class_init(ChmseeClass *);
static void chmsee_init(Chmsee *);
static void chmsee_finalize(GObject *);
static void chmsee_dispose(GObject *);

static gboolean delete_cb(GtkWidget *, GdkEvent *, Chmsee *);
static void destroy_cb(GtkWidget *, Chmsee *);
static gboolean window_state_event_cb(Chmsee *, GdkEventWindowState *);
static gboolean configure_event_cb(GtkWidget *, GdkEventConfigure *, Chmsee *);
static void book_model_changed_cb(Chmsee *, CsChmfile *, const gchar *);
static void book_html_changed_cb(Chmsee *, CsBook *);
static void book_message_notify_cb(Chmsee *, GParamSpec *, CsBook *);

static void open_file_response_cb(GtkWidget *, gint, Chmsee *);
static void about_response_cb(GtkDialog *, gint, gpointer);

static void on_open_file(GtkWidget *, Chmsee *);
static void on_recent_files(GtkRecentChooser *, Chmsee *);
static void on_open_new_tab(GtkWidget *, Chmsee *);
static void on_close_current_tab(GtkWidget *, Chmsee *);

static void on_menu_file(GtkWidget *, Chmsee *);
static void on_menu_edit(GtkWidget *, Chmsee *);
static void on_home(GtkWidget *, Chmsee *);
static void on_back(GtkWidget *, Chmsee *);
static void on_forward(GtkWidget *, Chmsee *);
static void on_zoom_in(GtkWidget *, Chmsee *);
static void on_zoom_reset(GtkWidget *, Chmsee *);
static void on_zoom_out(GtkWidget *, Chmsee *);
static void on_setup(GtkWidget *, Chmsee *);
static void on_about(GtkWidget *);
static void on_copy(GtkWidget *, Chmsee *);
static void on_select_all(GtkWidget *, Chmsee *);
static void on_find(GtkWidget *, Chmsee *);
static void on_keyboard_escape(GtkWidget *, Chmsee *);
static void on_fullscreen_toggled(GtkWidget *, Chmsee *);
static void on_sidepane_toggled(GtkWidget *, Chmsee *);

static void chmsee_quit(Chmsee *);
static void populate_windows(Chmsee *);
static void chmsee_set_fullscreen(Chmsee *, gboolean);
static void show_sidepane(Chmsee *);
static void hide_sidepane(Chmsee *);
static void set_sidepane_state(Chmsee *, gboolean);
static void chmsee_open_draged_file(Chmsee *, const gchar *);
static void chmsee_drag_data_received(GtkWidget *, GdkDragContext *, gint, gint,
                                      GtkSelectionData *, guint, guint);
static void update_status_bar(Chmsee *, const gchar *);
static void fullscreen(Chmsee *);
static void unfullscreen(Chmsee *);

/* Normal items */
static const GtkActionEntry entries[] = {
        { "FileMenu", NULL, N_("_File"), NULL, NULL, G_CALLBACK(on_menu_file) },
        { "EditMenu", NULL, N_("_Edit"), NULL, NULL, G_CALLBACK(on_menu_edit) },
        { "ViewMenu", NULL, N_("_View") },
        { "HelpMenu", NULL, N_("_Help") },

        { "Open", GTK_STOCK_OPEN, N_("_Open"), "<control>O", N_("Open a file"), G_CALLBACK(on_open_file)},
        { "RecentFiles", NULL, N_("_Recent Files"), NULL, NULL, NULL},

        { "NewTab", NULL, N_("New _Tab"), "<control>T", NULL, G_CALLBACK(on_open_new_tab)},
        { "CloseTab", NULL, N_("_Close Tab"), "<control>W", NULL, G_CALLBACK(on_close_current_tab)},
        { "Exit", GTK_STOCK_QUIT, N_("E_xit"), "<control>Q", N_("Exit ChmSee"), G_CALLBACK(destroy_cb)},

        { "Copy", NULL, N_("_Copy"), NULL, NULL, G_CALLBACK(on_copy)},
        { "SelectAll", NULL, N_("Select _All"), NULL, NULL, G_CALLBACK(on_select_all)},

        { "Find", GTK_STOCK_FIND, N_("_Find"), "<control>F", NULL, G_CALLBACK(on_find)},

        { "Preferences", GTK_STOCK_PREFERENCES, N_("_Preferences"), NULL, N_("Preferences"), G_CALLBACK(on_setup)},

        { "Home", GTK_STOCK_HOME, N_("_Home"), NULL, NULL, G_CALLBACK(on_home)},
        { "Back", GTK_STOCK_GO_BACK, N_("_Back"), "<alt>Left", NULL, G_CALLBACK(on_back)},
        { "Forward", GTK_STOCK_GO_FORWARD, N_("_Forward"), "<alt>Right", NULL, G_CALLBACK(on_forward)},

        { "About", GTK_STOCK_ABOUT, N_("_About"), NULL, N_("About ChmSee"), G_CALLBACK(on_about)},

        { "ZoomIn", GTK_STOCK_ZOOM_IN, N_("Zoom _In"), "<control>plus", NULL, G_CALLBACK(on_zoom_in)},
        { "ZoomReset", GTK_STOCK_ZOOM_100, N_("_Normal Size"), "<control>0", NULL, G_CALLBACK(on_zoom_reset)},
        { "ZoomOut", GTK_STOCK_ZOOM_OUT, N_("Zoom _Out"), "<control>minus", NULL, G_CALLBACK(on_zoom_out)},

        { "OnKeyboardEscape", NULL, NULL, "Escape", NULL, G_CALLBACK(on_keyboard_escape)},
        { "OnKeyboardControlEqual", NULL, NULL, "<control>equal", NULL, G_CALLBACK(on_zoom_in)}
};

/* Toggle items */
static const GtkToggleActionEntry toggle_entries[] = {
        { "FullScreen", NULL, N_("Full _Screen"), "F11", "Switch between full screen and windowed mode", G_CALLBACK(on_fullscreen_toggled), FALSE },
        { "SidePane", NULL, N_("Side _Pane"), "F9", NULL, G_CALLBACK(on_sidepane_toggled), FALSE }
};

/* Radio items */
static const GtkRadioActionEntry radio_entries[] = {
};

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
chmsee_init(Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        priv->chmfile = NULL;
        priv->config  = NULL;
        priv->state = CHMSEE_STATE_INIT;

        gtk_widget_add_events(GTK_WIDGET(self),
                              GDK_STRUCTURE_MASK | GDK_BUTTON_PRESS_MASK );

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
                         "delete-event",
                         G_CALLBACK (delete_cb),
                         self);
        g_signal_connect(G_OBJECT (self),
                         "destroy-event",
                         G_CALLBACK (destroy_cb),
                         self);

        /* startup html render engine */
        if(!cs_html_gecko_init_system()) {
                g_error("Initialize html render engine failed!");
                exit(1);
        }

        populate_windows(self);
}

static void
chmsee_dispose(GObject *gobject)
{
        g_debug("Chmsee >>> dispose");
        Chmsee        *self = CHMSEE(gobject);
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        if (priv->chmfile) {
                g_object_unref(priv->chmfile);
                priv->chmfile = NULL;
        }

        if (priv->action_group) {
                g_object_unref(priv->action_group);
                g_object_unref(priv->ui_manager);
                priv->action_group = NULL;
                priv->ui_manager = NULL;
        }

        G_OBJECT_CLASS(chmsee_parent_class)->dispose(gobject);
}

static void
chmsee_finalize(GObject *object)
{
        g_debug("Chmsee >>> finalize");

        G_OBJECT_CLASS (chmsee_parent_class)->finalize (object);
}

/* Callbacks */

static gboolean
delete_cb(GtkWidget *widget, GdkEvent *event, Chmsee *self)
{
        g_debug("Chmsee >>> window delete");
        chmsee_quit(self);
        return FALSE;
}

static void
destroy_cb(GtkWidget *widget, Chmsee *self)
{
        g_debug("Chmsee >>> window destroy");
        chmsee_quit(self);
}

static gboolean
window_state_event_cb(Chmsee *self, GdkEventWindowState *event)
{
        g_return_val_if_fail(IS_CHMSEE(self), FALSE);
        g_return_val_if_fail(event->type == GDK_WINDOW_STATE, FALSE);

        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        g_debug("Chmsee >>> on_window_state_event with event->changed_mask = %d and event->new_window_state = %d",
                event->changed_mask,
                event->new_window_state
                );

        if(!(event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)) {
                return FALSE;
        }

        if(event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) {
                if(priv->config->fullscreen) {
                        fullscreen(self);
                } else {
                        g_warning("expect not fullscreen but got a fullscreen event, restored");
                        chmsee_set_fullscreen(self, FALSE);
                        return TRUE;
                }
        } else {
                if(!priv->config->fullscreen) {
                        unfullscreen(self);
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

        if (!priv->config->fullscreen) {
                priv->config->width  = event->width;
                priv->config->height = event->height;
                priv->config->pos_x  = event->x;
                priv->config->pos_y  = event->y;
        }

        return FALSE;
}

static void
book_model_changed_cb(Chmsee *self, CsChmfile *chmfile, const gchar *filename)
{
        g_debug("Chmsee >>> receive book model changed callback %s", filename);
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        gboolean has_model = (chmfile != NULL);

        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "NewTab"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "CloseTab"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "SelectAll"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "Home"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "Find"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "SidePane"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "ZoomIn"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "ZoomOut"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "ZoomReset"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "Back"), has_model);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "Forward"), has_model);

        gtk_widget_set_sensitive(priv->book, has_model);

        if (filename
            && g_str_has_prefix(filename, "file://")
            && (g_str_has_suffix(filename, ".chm") || g_str_has_suffix(filename, ".CHM"))) {
                chmsee_open_draged_file(self, filename);
        }
}

static void
book_html_changed_cb(Chmsee *self, CsBook *book)
{
        g_debug("Chmsee >>> recieve html_changed signal");

        gboolean home_state, back_state, forward_state;
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        home_state = cs_book_has_homepage(book);
        back_state = cs_book_can_go_back(book);
        forward_state = cs_book_can_go_forward(book);

        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "Home"), home_state);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "Back"), back_state);
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "Forward"), forward_state);
}

static void
book_message_notify_cb(Chmsee *self, GParamSpec *pspec, CsBook *book)
{
        gchar *message;
        g_object_get(book, "book-message", &message, NULL);

        g_debug("Chmsee >>> book message notify %s", message);
        update_status_bar(self, message);
        g_free(message);
}

static void
open_file_response_cb(GtkWidget *widget, gint response_id, Chmsee *self)
{
        gchar *filename = NULL;

        if (response_id == GTK_RESPONSE_OK)
                filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (widget));

        gtk_widget_destroy(widget);

        if (filename != NULL)
                chmsee_open_file(self, filename);

        g_free(filename);
}

/* Toolbar button events */

static void
on_open_file(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        /* create open file dialog */
        GtkBuilder *builder = gtk_builder_new();
        gtk_builder_add_from_file(builder, RESOURCE_FILE ("openfile-dialog.ui"), NULL);

        GtkWidget *dialog = BUILDER_WIDGET (builder, "openfile_dialog");

        g_signal_connect(G_OBJECT (dialog),
                         "response",
                         G_CALLBACK (open_file_response_cb),
                         self);

        /* file list fiter */
        GtkFileFilter *filter;
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, _("CHM Files"));
        gtk_file_filter_add_pattern(filter, "*.[cC][hH][mM]");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER (dialog), filter);

        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, _("All Files"));
        gtk_file_filter_add_pattern(filter, "*");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER (dialog), filter);

        /* previous opened file folder */
        gchar *last_dir = NULL;
        if (priv->config->last_file) {
                last_dir = g_path_get_dirname(priv->config->last_file);
        } else {
                last_dir = g_strdup(g_get_home_dir());
        }
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (dialog), last_dir);
        g_free(last_dir);

        g_object_unref(G_OBJECT (builder));
}

static void
on_recent_files(GtkRecentChooser *chooser, Chmsee *self)
{
        gchar *uri = gtk_recent_chooser_get_current_uri(chooser);

        if (uri != NULL)
        {
                gchar *filename = g_filename_from_uri(uri, NULL, NULL);

                chmsee_open_file(self, filename);
                g_free(filename);
        }
        g_free(uri);
}

static void
on_copy(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_book_copy(CS_BOOK (priv->book));
}

static void
on_select_all(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_book_select_all(CS_BOOK (priv->book));
}

static void
on_setup(GtkWidget *widget, Chmsee *self)
{
        setup_window_new(self);
}

static void
on_back(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_book_go_back(CS_BOOK (priv->book));
}

static void
on_forward(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_book_go_forward(CS_BOOK (priv->book));
}

static void
on_menu_file(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        gboolean can_close_tab = cs_book_can_close_tab(CS_BOOK (priv->book));
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "CloseTab"), can_close_tab);
}

static void
on_menu_edit(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        gboolean can_copy = cs_book_can_copy(CS_BOOK (priv->book));
        gtk_action_set_sensitive(gtk_action_group_get_action(priv->action_group, "Copy"), can_copy);
}

static void
on_home(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_book_homepage(CS_BOOK (priv->book));
}

static void
on_zoom_in(GtkWidget *widget, Chmsee *self)
{
        cs_book_zoom_in(CS_BOOK (CHMSEE_GET_PRIVATE (self)->book));
}

static void
on_zoom_out(GtkWidget *widget, Chmsee *self)
{
        cs_book_zoom_out(CS_BOOK (CHMSEE_GET_PRIVATE (self)->book));
}

static void
on_zoom_reset(GtkWidget *widget, Chmsee *self)
{
        cs_book_zoom_reset(CS_BOOK (CHMSEE_GET_PRIVATE (self)->book));
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
        gtk_builder_add_from_file(builder, RESOURCE_FILE ("about-dialog.ui"), NULL);

        GtkWidget *dialog = BUILDER_WIDGET (builder, "about_dialog");

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
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        gchar *location = cs_book_get_location(CS_BOOK (priv->book));
        cs_book_new_tab_with_fulluri(CS_BOOK (priv->book), location);
        g_free(location);
}

static void
on_close_current_tab(GtkWidget *widget, Chmsee *self)
{
        cs_book_close_current_tab(CS_BOOK (CHMSEE_GET_PRIVATE (self)->book));
}

static void
on_keyboard_escape(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        cs_book_findbar_hide(CS_BOOK (priv->book));
}

static void
on_fullscreen_toggled(GtkWidget *menu, Chmsee *self)
{
        g_return_if_fail(IS_CHMSEE(self));

        gboolean active;
        g_object_get(G_OBJECT (menu),
                     "active", &active,
                     NULL);
        g_debug("Chmsee >>> enter on_fullscreen_toggled with menu.active = %d", active);
        chmsee_set_fullscreen(self, active);
}

static void
on_sidepane_toggled(GtkWidget *menu, Chmsee *self)
{
        g_return_if_fail(IS_CHMSEE (self));

        gboolean active;
        g_object_get(G_OBJECT (menu),
                     "active", &active,
                     NULL);
        if (active) {
                show_sidepane(self);
        } else {
                hide_sidepane(self);
        }
}

static void
on_find(GtkWidget *widget, Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_book_findbar_show(CS_BOOK (priv->book));
}

/* internal functions */

static void
chmsee_quit(Chmsee *self)
{
        g_message("Chmsee >>> quit");

        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        cs_html_gecko_shutdown_system();
        priv->config->hpaned_pos = cs_book_get_hpaned_position(CS_BOOK (priv->book));
        gtk_widget_destroy(GTK_WIDGET (self));
        gtk_main_quit();
}

static void
chmsee_drag_data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                           GtkSelectionData *selection_data, guint info, guint time)
{
        gchar  **uris;
        gint     i = 0;

        uris = gtk_selection_data_get_uris(selection_data);
        if (!uris) {
                gtk_drag_finish (context, FALSE, FALSE, time);
                return;
        }

        for (i = 0; uris[i]; i++) {
                gchar *uri = uris[i];
                if (g_str_has_prefix(uri, "file://")
                    && (g_str_has_suffix(uri, ".chm") || g_str_has_suffix(uri, ".CHM"))) {
                        chmsee_open_draged_file(CHMSEE (widget), uri);
                        break;
                }
        }

        gtk_drag_finish(context, TRUE, FALSE, time);

        g_strfreev(uris);
}

static void
chmsee_open_draged_file(Chmsee *chmsee, const gchar *file)
{
        gchar *fname = g_uri_unescape_string(file+7, NULL);
        chmsee_open_file(chmsee, fname);
        g_free(fname);
}

static void
populate_windows(Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        GtkWidget *vbox = GTK_WIDGET (gtk_vbox_new(FALSE, 2));

        GtkActionGroup *action_group = gtk_action_group_new("MenuActions");
        priv->action_group = action_group;
        gtk_action_group_set_translation_domain(priv->action_group, NULL);
        gtk_action_group_add_actions(action_group, entries, G_N_ELEMENTS (entries), self);
        gtk_action_group_add_toggle_actions(action_group, toggle_entries, G_N_ELEMENTS (toggle_entries), self);

        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "NewTab"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "CloseTab"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "SelectAll"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "Find"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "Home"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "Back"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "Forward"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "SidePane"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "ZoomIn"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "ZoomOut"), FALSE);
        gtk_action_set_sensitive(gtk_action_group_get_action(action_group, "ZoomReset"), FALSE);

        GtkUIManager *ui_manager = gtk_ui_manager_new();
        priv->ui_manager = ui_manager;
        gtk_ui_manager_insert_action_group(ui_manager, action_group, 0);

        GtkAccelGroup *accel_group = gtk_ui_manager_get_accel_group(ui_manager);
        gtk_window_add_accel_group(GTK_WINDOW (self), accel_group);

        GError *error = NULL;
        if (!gtk_ui_manager_add_ui_from_string(ui_manager, ui_description, -1, &error))
        {
                g_warning ("Chmsee >>> building menus failed: %s", error->message);
                g_error_free (error);
                exit (EXIT_FAILURE);
        }

        GtkWidget *menubar = gtk_handle_box_new();
        priv->menubar = menubar;
        gtk_container_add(GTK_CONTAINER(menubar), gtk_ui_manager_get_widget (ui_manager, "/MainMenu"));
        gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, FALSE, 0);

        GtkRecentManager *manager = gtk_recent_manager_get_default();
        GtkWidget *recent_menu = gtk_recent_chooser_menu_new_for_manager(manager);

        gtk_recent_chooser_set_show_not_found(GTK_RECENT_CHOOSER (recent_menu), FALSE);
        gtk_recent_chooser_set_local_only(GTK_RECENT_CHOOSER (recent_menu), TRUE);
        gtk_recent_chooser_set_limit(GTK_RECENT_CHOOSER (recent_menu), 10);
        gtk_recent_chooser_set_show_icons(GTK_RECENT_CHOOSER (recent_menu), FALSE);
        gtk_recent_chooser_set_sort_type(GTK_RECENT_CHOOSER (recent_menu), GTK_RECENT_SORT_MRU);
        gtk_recent_chooser_menu_set_show_numbers(GTK_RECENT_CHOOSER_MENU (recent_menu), TRUE);

        GtkRecentFilter *filter = gtk_recent_filter_new();
        gtk_recent_filter_add_application(filter, g_get_application_name());
        gtk_recent_chooser_set_filter(GTK_RECENT_CHOOSER (recent_menu), filter);

        g_signal_connect(recent_menu,
                         "item-activated",
                         G_CALLBACK (on_recent_files),
                         self);

        GtkWidget *widget = gtk_ui_manager_get_widget(ui_manager, "/MainMenu/FileMenu/RecentFiles");
        gtk_menu_item_set_submenu(GTK_MENU_ITEM (widget), recent_menu);

        GtkWidget *toolbar = gtk_handle_box_new();
        priv->toolbar = toolbar;
        gtk_container_add(GTK_CONTAINER(toolbar), gtk_ui_manager_get_widget(ui_manager, "/toolbar"));
        gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
        /* gtk_toolbar_set_style(GTK_TOOLBAR (gtk_ui_manager_get_widget(ui_manager, "/toolbar")), */
        /*                       GTK_TOOLBAR_ICONS);// FIXME: issue 43 */
        gtk_tool_button_set_icon_widget(
                GTK_TOOL_BUTTON(gtk_ui_manager_get_widget(ui_manager, "/toolbar/sidepane")),
                gtk_image_new_from_file(RESOURCE_FILE ("show-pane.png")));

        priv->book = cs_book_new();
        gtk_box_pack_start(GTK_BOX(vbox), priv->book, TRUE, TRUE, 0);
        g_signal_connect_swapped(priv->book,
                                 "model-changed",
                                 G_CALLBACK (book_model_changed_cb),
                                 self);

        /* status bar */
        priv->statusbar = gtk_statusbar_new();
        gtk_box_pack_start(GTK_BOX(vbox), priv->statusbar, FALSE, FALSE, 0);

        priv->scid_default = gtk_statusbar_get_context_id(GTK_STATUSBAR (priv->statusbar), "default");

        gtk_container_add(GTK_CONTAINER (self), vbox);

        accel_group = g_object_new(GTK_TYPE_ACCEL_GROUP, NULL);
        gtk_window_add_accel_group(GTK_WINDOW (self), accel_group);

        update_status_bar(self, _("Ready!"));
        gtk_widget_show_all(GTK_WIDGET (self));
        cs_book_findbar_hide(CS_BOOK (priv->book));
        g_debug("Chmsee >>> populate window finished.");
}

static void
chmsee_set_fullscreen(Chmsee *self, gboolean fullscreen)
{
        g_debug("Chmsee >>> chmsee_set_fullscreen with fullscreen = %d", fullscreen);

        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        priv->config->fullscreen = fullscreen;

        if(fullscreen) {
                g_debug("Chmsee >>> call gtk_window_fullscreen");
                gtk_window_fullscreen(GTK_WINDOW(self));
        } else {
                g_debug("ChmSee >>> call gtk_window_unfullscreen");
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
set_sidepane_state(Chmsee *self, gboolean state)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        GtkWidget *icon_widget;
        g_object_set(priv->book,
                     "sidepane-visible", state,
                     NULL);

        if (state) {
                icon_widget = gtk_image_new_from_file(RESOURCE_FILE ("hide-pane.png"));
        } else {
                icon_widget = gtk_image_new_from_file(RESOURCE_FILE ("show-pane.png"));
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
fullscreen(Chmsee *self)
{
        g_debug("Chmsee >>> enter fullscreen");
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        priv->config->fullscreen = TRUE;
        gtk_widget_hide(priv->menubar);
        gtk_widget_hide(priv->toolbar);
        gtk_widget_hide(priv->statusbar);
}

static void
unfullscreen(Chmsee *self)
{
        g_message("Chmsee >>> enter unfullscreen");
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        priv->config->fullscreen = FALSE;
        gtk_widget_show(priv->menubar);
        gtk_widget_show(priv->toolbar);
        gtk_widget_show(priv->statusbar);
}

/* External functions */

Chmsee *
chmsee_new(CsConfig *config)
{
        Chmsee        *self = g_object_new(CHMSEE_TYPE, NULL);
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        priv->config = config;

        cs_html_gecko_set_default_lang(config->lang);

        if (config->pos_x >= 0 && config->pos_y >= 0)
                gtk_window_move(GTK_WINDOW (self), config->pos_x, config->pos_y);

        if (config->width > 0 && config->height > 0)
                gtk_window_resize(GTK_WINDOW (self), config->width, config->height);
        else
                gtk_window_resize(GTK_WINDOW (self), 800, 600);

        gtk_window_set_title(GTK_WINDOW (self), "ChmSee");
        gtk_window_set_icon_from_file(GTK_WINDOW (self), RESOURCE_FILE ("chmsee-icon.png"), NULL);

        cs_book_set_hpaned_position(CS_BOOK (priv->book), config->hpaned_pos);
        hide_sidepane(self);

        /* widget size changed event handle */
        g_signal_connect(G_OBJECT (self),
                         "configure-event",
                         G_CALLBACK (configure_event_cb),
                         self);

        g_message("Chmsee >>> created");
        return self;
}

void
chmsee_open_file(Chmsee *self, const gchar *filename)
{
        g_return_if_fail(IS_CHMSEE (self));

        g_message("Chmsee >>> open file = %s", filename);
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        /* close currently opened book */
        if (priv->chmfile) {
                g_object_unref(priv->chmfile);
        }

        /* create chmfile, get file infomation */
        priv->chmfile = cs_chmfile_new(filename, priv->config->bookshelf);

        if (priv->chmfile) {
                priv->state = CHMSEE_STATE_LOADING;

                cs_book_set_model(CS_BOOK (priv->book), priv->chmfile);

                gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON (gtk_ui_manager_get_widget(priv->ui_manager, "/toolbar/sidepane")), TRUE);
                gtk_container_set_focus_child(GTK_CONTAINER(self), priv->book);

                g_signal_connect_swapped(priv->book,
                                         "html-changed",
                                         G_CALLBACK (book_html_changed_cb),
                                         self);
                g_signal_connect_swapped(priv->book,
                                         "notify::book-message",
                                         G_CALLBACK (book_message_notify_cb),
                                         self);

                /* update window title */
                gchar *window_title = g_strdup_printf("%s - ChmSee", cs_chmfile_get_bookname(priv->chmfile));
                g_debug("Chmsee >>> update window title %s", window_title);
                gtk_window_set_title(GTK_WINDOW (self), window_title);
                g_free(window_title);

                /* record last opened file directory */
                if (priv->config->last_file)
                        g_free(priv->config->last_file);

                priv->config->last_file = g_strdup(cs_chmfile_get_filename(priv->chmfile));
                g_debug("Chmsee >>> record last file =  %s", priv->config->last_file);

                /* recent files */
                gchar *content;
                gsize length;

                if (g_file_get_contents(filename, &content, &length, NULL)) {
                        static gchar *groups[2] = {
                                "CHM Viewer",
                                NULL
                        };

                        GtkRecentData *data = g_slice_new(GtkRecentData);
                        data->display_name = NULL;
                        data->description = NULL;
                        data->mime_type = "application/x-chm";
                        data->app_name = (gchar*) g_get_application_name();
                        data->app_exec = g_strjoin(" ", g_get_prgname (), "%u", NULL);
                        data->groups = groups;
                        data->is_private = FALSE;
                        gchar *uri = g_filename_to_uri(filename, NULL, NULL);

                        GtkRecentManager *manager = gtk_recent_manager_get_default();
                        gtk_recent_manager_add_full(manager, uri, data);

                        g_free(uri);
                        g_free(data->app_exec);
                        g_slice_free(GtkRecentData, data);
                }

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

int
chmsee_get_lang(Chmsee *self)
{
        return CHMSEE_GET_PRIVATE (self)->config->lang;
}

void
chmsee_set_lang(Chmsee *self, int lang)
{
        CHMSEE_GET_PRIVATE (self)->config->lang = lang;
}

gboolean
chmsee_get_startup_lastfile(Chmsee *self)
{
        return CHMSEE_GET_PRIVATE (self)->config->startup_lastfile;
}

void
chmsee_set_startup_lastfile(Chmsee *self, gboolean state)
{
        g_debug("Chmsee >>> set startup lastfile = %d", state);
        CHMSEE_GET_PRIVATE (self)->config->startup_lastfile = state;
}

const gchar *
chmsee_get_variable_font(Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        return cs_chmfile_get_variable_font(priv->chmfile);
}

void
chmsee_set_variable_font(Chmsee *self, const gchar *font_name)
{
        g_debug("Chmsee >>> set variable font = %s", font_name);
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_html_gecko_set_variable_font(font_name);
        cs_chmfile_set_variable_font(priv->chmfile, font_name);
}

const gchar *
chmsee_get_fixed_font(Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        return cs_chmfile_get_fixed_font(priv->chmfile);
}

void
chmsee_set_fixed_font(Chmsee *self, const gchar *font_name)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        cs_html_gecko_set_fixed_font(font_name);
        cs_chmfile_set_fixed_font(priv->chmfile, font_name);
}

gboolean
chmsee_has_book(Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        return priv->chmfile != NULL;
}

void
chmsee_close_book(Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);

        if (priv->chmfile) {
                g_object_unref(priv->chmfile);
                priv->chmfile = NULL;
        }

        book_model_changed_cb(self, NULL, NULL);

        priv->state = CHMSEE_STATE_NORMAL;
}

const gchar *
chmsee_get_bookshelf(Chmsee *self)
{
        ChmseePrivate *priv = CHMSEE_GET_PRIVATE (self);
        return priv->config->bookshelf;
}

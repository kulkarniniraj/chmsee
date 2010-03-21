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

#include "setup.h"
#include "utils.h"

static void on_bookshelf_clear(GtkWidget *, Chmsee *);
static void on_window_close(GtkButton *, Chmsee *);

static void variable_font_set_cb(GtkFontButton *, Chmsee *);
static void fixed_font_set_cb(GtkFontButton *, Chmsee *);
static void cmb_lang_changed_cb(GtkWidget *, Chmsee *);

static void
on_bookshelf_clear(GtkWidget *widget, Chmsee *chmsee)
{
        char *argv[4];
        gchar *bookshelf = g_strdup(chmsee_get_bookshelf(chmsee));

        chmsee_close_book(chmsee);

        g_return_if_fail(g_file_test(bookshelf, G_FILE_TEST_EXISTS));

        argv[0] = "rm";
        argv[1] = "-rf";
        argv[2] = bookshelf;
        argv[3] = NULL;

        g_spawn_async(g_get_tmp_dir(), argv, NULL,
                      G_SPAWN_SEARCH_PATH,
                      NULL, NULL, NULL,
                      NULL);
        g_free(bookshelf);
}

static void
on_window_close(GtkButton *button, Chmsee *chmsee)
{
        gtk_widget_destroy(gtk_widget_get_toplevel (GTK_WIDGET(button)));
}

static void
variable_font_set_cb(GtkFontButton *button, Chmsee *chmsee)
{
        gchar *font_name = g_strdup(gtk_font_button_get_font_name(button));

        g_debug("SETUP >>> variable font set: %s", font_name);

        chmsee_set_variable_font(chmsee, font_name);
        g_free(font_name);
}

static void
fixed_font_set_cb(GtkFontButton *button, Chmsee *chmsee)
{
        gchar *font_name = g_strdup(gtk_font_button_get_font_name(button));

        g_debug("SETUP >>> fixed font set: %s", font_name);

        chmsee_set_fixed_font(chmsee, font_name);
        g_free(font_name);
}

static void
cmb_lang_changed_cb(GtkWidget *widget, Chmsee *chmsee)
{

        GtkComboBox *combobox = GTK_COMBO_BOX (widget);
        gint index = gtk_combo_box_get_active(combobox);

        if (index >= 0) {
                g_debug("SETUP >>> select lang: %d", index);
                chmsee_set_lang(chmsee, index);
        }
}

void
setup_window_new(Chmsee *chmsee)
{
        g_debug("SETUP >>> create setup window");
        /* create setup window */
        GtkBuilder *builder = gtk_builder_new();
        gtk_builder_add_from_file(builder, RESOURCE_FILE ("setup-window.ui"), NULL);

        GtkWidget *setup_window = BUILDER_WIDGET (builder, "setup_window");

        g_signal_connect_swapped((gpointer) setup_window,
                                 "destroy",
                                 G_CALLBACK (gtk_widget_destroy),
                                 GTK_OBJECT (setup_window));

        /* bookshelf directory */
        GtkWidget *bookshelf_entry = BUILDER_WIDGET (builder, "bookshelf_entry");
        gtk_entry_set_text(GTK_ENTRY(bookshelf_entry), chmsee_get_bookshelf(chmsee));

        GtkWidget *clear_button = BUILDER_WIDGET (builder, "setup_clear");
        g_signal_connect(G_OBJECT (clear_button),
                         "clicked",
                         G_CALLBACK (on_bookshelf_clear),
                         chmsee);

        /* font setting */
        GtkWidget *variable_font_button = BUILDER_WIDGET (builder, "variable_fontbtn");
        g_signal_connect(G_OBJECT (variable_font_button),
                         "font-set",
                         G_CALLBACK (variable_font_set_cb),
                         chmsee);

        GtkWidget *fixed_font_button = BUILDER_WIDGET (builder, "fixed_fontbtn");
        g_signal_connect(G_OBJECT (fixed_font_button),
                         "font-set",
                         G_CALLBACK (fixed_font_set_cb),
                         chmsee);

        /* default lang */
        GtkWidget *cmb_lang = BUILDER_WIDGET (builder, "cmb_default_lang");
        g_signal_connect(G_OBJECT (cmb_lang),
                         "changed",
                         G_CALLBACK (cmb_lang_changed_cb),
                         chmsee);
        gtk_combo_box_set_active(GTK_COMBO_BOX (cmb_lang), chmsee_get_lang(chmsee));

        GtkWidget *close_button = BUILDER_WIDGET (builder, "setup_close");
        g_signal_connect(G_OBJECT (close_button),
                         "clicked",
                         G_CALLBACK (on_window_close),
                         chmsee);

        if (chmsee_has_book(chmsee)) {
                gtk_font_button_set_font_name(GTK_FONT_BUTTON (variable_font_button),
                                              chmsee_get_variable_font(chmsee));
                gtk_font_button_set_font_name(GTK_FONT_BUTTON (fixed_font_button),
                                              chmsee_get_fixed_font(chmsee));
                gtk_widget_set_sensitive(variable_font_button, TRUE);
                gtk_widget_set_sensitive(fixed_font_button, TRUE);
        }

        g_object_unref(builder);
}

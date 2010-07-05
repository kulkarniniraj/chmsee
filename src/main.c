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

#include <stdlib.h>
#include <getopt.h>
#include <libintl.h>
#include <sys/stat.h>

#include <glib.h>

#include <errno.h>
#include <pthread.h>

#include "chmsee.h"
#include "startup.h"
#include "utils.h"

static int log_level = 2; /* only show WARNING, CRITICAL, ERROR */

static void
dummy_log_handler (const gchar *log_domain,
                   GLogLevelFlags log_level,
                   const gchar *message,
                   gpointer unused_data)
{}

static void
init_log(int log_level)
{
        if (log_level < 1) g_log_set_handler(NULL, G_LOG_LEVEL_CRITICAL, dummy_log_handler, NULL);
        if (log_level < 2) g_log_set_handler(NULL, G_LOG_LEVEL_WARNING,  dummy_log_handler, NULL);
        if (log_level < 3) g_log_set_handler(NULL, G_LOG_LEVEL_MESSAGE,  dummy_log_handler, NULL);
        if (log_level < 4) g_log_set_handler(NULL, G_LOG_LEVEL_INFO,     dummy_log_handler, NULL);
        if (log_level < 5) g_log_set_handler(NULL, G_LOG_LEVEL_DEBUG,    dummy_log_handler, NULL);
}

static gboolean
callback_verbose(const gchar *option_name,
                 const gchar *value,
                 gpointer data,
                 GError **error)
{
        log_level++;
        return TRUE;
}

static gboolean
callback_quiet(const gchar *option_name,
               const gchar *value,
               gpointer data,
               GError **error)
{
        log_level--;
        return TRUE;
}

CsConfig *
load_config()
{
        g_message("Main >>> load config");
        CsConfig *config = g_slice_new(CsConfig);

        /* ChmSee HOME directory ~/.chmsee */
        config->home = g_build_filename(g_get_home_dir(), ".chmsee", NULL);
        if (!g_file_test(config->home, G_FILE_TEST_IS_DIR))
                mkdir(config->home, 0755);

        config->bookshelf = g_build_filename(config->home, CHMSEE_BOOKSHELF_DEFAULT, NULL);
        if (!g_file_test(config->bookshelf, G_FILE_TEST_IS_DIR))
                mkdir(config->bookshelf, 0755);

        config->last_file  = NULL;
        config->pos_x      = -100;
        config->pos_y      = -100;
        config->width      = 0;
        config->height     = 0;
        config->hpaned_pos = 200;
        config->fullscreen       = FALSE;
        config->startup_lastfile = FALSE;
        config->splash = TRUE;

        gchar *config_file = g_build_filename(config->home, CHMSEE_CONFIG_FILE, NULL);
        g_debug("Main >>> chmsee config file path = %s", config_file);
        if (!g_file_test(config_file, G_FILE_TEST_EXISTS))
                return config;

        GKeyFile *keyfile = g_key_file_new();
        gboolean rv = g_key_file_load_from_file(keyfile, config_file, G_KEY_FILE_NONE, NULL);

        if (!rv) {
                convert_old_config_file(config_file, "[ChmSee]\n");
        }

        rv = g_key_file_load_from_file(keyfile, config_file, G_KEY_FILE_NONE, NULL);

        if (rv) {
                config->last_file = g_key_file_get_string (keyfile, "ChmSee", "LAST_FILE", NULL);

                config->pos_x      = g_key_file_get_integer(keyfile, "ChmSee", "POS_X", NULL);
                config->pos_y      = g_key_file_get_integer(keyfile, "ChmSee", "POS_Y", NULL);
                config->width      = g_key_file_get_integer(keyfile, "ChmSee", "WIDTH", NULL);
                config->height     = g_key_file_get_integer(keyfile, "ChmSee", "HEIGHT", NULL);
                config->hpaned_pos = g_key_file_get_integer(keyfile, "ChmSee", "HPANED_POSITION", NULL);
                config->fullscreen       = g_key_file_get_boolean(keyfile, "ChmSee", "FULLSCREEN", NULL);
                config->startup_lastfile = g_key_file_get_boolean(keyfile, "ChmSee", "STARTUP_LASTFILE", NULL);
                config->splash           = g_key_file_get_boolean(keyfile, "ChmSee", "SPLASH", NULL);

                if (!config->hpaned_pos)
                        config->hpaned_pos = 200;
        }

        g_key_file_free(keyfile);
        g_free(config_file);

        return config;
}

void
save_config(CsConfig *config)
{
        gsize length = 0;

        g_message("Main >>> save config");
        gchar *config_file = g_build_filename(config->home, CHMSEE_CONFIG_FILE, NULL);

        GKeyFile *keyfile = g_key_file_new();

        if (config->last_file)
                g_key_file_set_string (keyfile, "ChmSee", "LAST_FILE", config->last_file);

        g_key_file_set_integer(keyfile, "ChmSee", "POS_X", config->pos_x);
        g_key_file_set_integer(keyfile, "ChmSee", "POS_Y", config->pos_y);
        g_key_file_set_integer(keyfile, "ChmSee", "WIDTH", config->width);
        g_key_file_set_integer(keyfile, "ChmSee", "HEIGHT", config->height);
        g_key_file_set_integer(keyfile, "ChmSee", "HPANED_POSITION", config->hpaned_pos);
        g_key_file_set_boolean(keyfile, "ChmSee", "FULLSCREEN", config->fullscreen);
        g_key_file_set_boolean(keyfile, "ChmSee", "STARTUP_LASTFILE", config->startup_lastfile);
        g_key_file_set_boolean(keyfile, "ChmSee", "SPLASH", config->splash);

        gchar *contents = g_key_file_to_data(keyfile, &length, NULL);
        g_file_set_contents(config_file, contents, length, NULL);

        g_key_file_free(keyfile);
        g_free(contents);
        g_free(config_file);

        g_free(config->home);
        g_free(config->bookshelf);
        g_free(config->last_file);

        g_slice_free(CsConfig, config);
}

int
main(int argc, char *argv[])
{
        const gchar *filename = NULL;
        /* const gchar *bookshelf = NULL; */

        GError *error = NULL;
        gboolean option_version = FALSE;

        if (!g_thread_supported())
                g_thread_init(NULL);

        gdk_threads_init();

        GOptionEntry options[] = {
                {"version", 0,
                 0, G_OPTION_ARG_NONE, &option_version,
                 _("Display ChmSee version"),
                 NULL
                },
                {"verbose", 'v',
                 G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, (void*)callback_verbose,
                 _("Be verbose, repeat 3 times to get all infomation"),
                 NULL
                },
                {"quiet", 'q',
                 G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, (void*)callback_quiet,
                 _("Be quiet, repeat 2 times to disable all infomation"),
                 NULL
                },
                /* {"bookshelf", 0, */
                /*  G_OPTION_FLAG_FILENAME, G_OPTION_ARG_FILENAME, &bookshelf, */
                /*  "specify bookshelf directory, default is " CHMSEE_BOOKSHELF_DEFAULT, */
                /*  _("Bookshelf") */
                /* }, */
                {NULL}
        };

        char params[] =
                "[chmfile]\n"
                "\n"
                "GTK+ based CHM file viewer\n"
                "Example: chmsee Handbook.chm::toc.html";

        if (!gtk_init_with_args(&argc, &argv, params, options, GETTEXT_PACKAGE, &error)) {
                g_printerr("%s\n", error->message);
                return 1;
        }

        if (option_version) {
                g_print("%s\n", PACKAGE_STRING);
                return 0;
        }

        if (argc == 1) {
        } else if (argc == 2) {
                filename = argv[1];
        } else {
                g_printerr(_("more than 1 argument\n"));
                return 1;
        }

        init_log(log_level);

        /* i18n */
        bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
        bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
        textdomain(GETTEXT_PACKAGE);

        CsConfig *config = load_config();

        /* if (bookshelf != NULL) { */
        /*         config->bookshelf = g_strdup(bookshelf); */
        /* } */

        gdk_threads_enter();

        /* Show splash screen */
        if(config->splash)
                startup_popup_new();

        Chmsee *chmsee = chmsee_new(config);

        if (chmsee == NULL) {
                g_warning("Creating chmsee main window failed!");
                return 1;
        }

        if (filename)
                chmsee_open_file(chmsee, filename);
        else if (config->startup_lastfile && config->last_file)
                chmsee_open_file(chmsee, config->last_file);

        gtk_main();

        gdk_threads_leave();

        save_config(config);

        return 0;
}

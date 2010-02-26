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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib/gstdio.h>
#include <chm_lib.h>

#include "chmfile.h"
#include "utils.h"
#include "models/bookmarksfile.h"
#include "models/parser.h"

typedef struct _CsChmfilePrivate CsChmfilePrivate;

struct _CsChmfilePrivate {
        GNode       *toc_tree;
        GList       *index_list;
        GList       *bookmarks_list;

        gchar       *hhc;
        gchar       *hhk;
        gchar       *bookfolder;
        gchar       *filename;

        gchar       *bookname;
        gchar       *homepage;
        gchar       *encoding;
        gchar       *variable_font;
        gchar       *fixed_font;
};

struct extract_context
{
        const char *base_path;
        const char *hhc_file;
        const char *hhk_file;
};

#define CS_CHMFILE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CS_TYPE_CHMFILE, CsChmfilePrivate))

#define UINT16ARRAY(x) ((unsigned char)(x)[0] | ((u_int16_t)(x)[1] << 8))
#define UINT32ARRAY(x) (UINT16ARRAY(x) | ((u_int32_t)(x)[2] << 16)      \
                        | ((u_int32_t)(x)[3] << 24))

static void cs_chmfile_class_init(CsChmfileClass *);
static void cs_chmfile_init(CsChmfile *);
static void cs_chmfile_dispose(GObject *);
static void cs_chmfile_finalize(GObject *);

static void chmfile_file_info(CsChmfile *);
static void chmfile_system_info(struct chmFile *, CsChmfile *);
static void chmfile_windows_info(struct chmFile *, CsChmfile *);
static void chmfile_set_encoding(CsChmfile *, const char *);

static int dir_exists(const char *);
static int rmkdir(char *);
static int _extract_callback(struct chmFile *, struct chmUnitInfo *, void *);

static gboolean extract_chm(const gchar *, CsChmfile *);
static void load_bookinfo(CsChmfile *);
static void save_bookinfo(CsChmfile *);
static void extract_post_file_write(const gchar *);

static GNode *parse_hhc_file(const gchar *, const gchar*);
static GList *parse_hhk_file(const gchar *, const gchar*);

/* GObject functions */

G_DEFINE_TYPE (CsChmfile, chmfile, GTK_TYPE_OBJECT);

static void
cs_chmfile_class_init(CsChmfileClass *klass)
{
        g_type_class_add_private(klass, sizeof(CsChmfilePrivate));

        GObjectClass *object_class = G_OBJECT_CLASS(klass);

        object_class->dispose  = cs_chmfile_dispose;
        object_class->finalize = cs_chmfile_finalize;
}

static void
cs_chmfile_init(CsChmfile *self)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);

        priv->toc_tree       = NULL;
        priv->index_list     = NULL;
        priv->bookmarks_list = NULL;

        priv->hhc            = NULL;
        priv->hhk            = NULL;
        priv->bookfolder     = NULL;
        priv->filename       = NULL;

        priv->bookname       = NULL;
        priv->homepage       = NULL;

        priv->encoding       = g_strdup("UTF-8");
        priv->variable_font  = g_strdup("Sans 12");
        priv->fixed_font     = g_strdup("Monospace 12");
}

static void
cs_chmfile_dispose(GObject* object)
{
        CsChmfile* self = CHMFILE(object);
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);
        if(priv->index) {
                g_object_unref(priv->index);
                priv->index = NULL;
        }
}

static void
cs_chmfile_finalize(GObject *object)
{
        CsChmfile        *self = CS_CHMFILE (object);
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);

        g_message("CS_CHMFILE: finalize");

        save_bookinfo(self);

        gchar *bookmarks_file = g_build_filename(priv->bookfolder, CHMSEE_BOOKMARKS_FILE, NULL);
        cs_bookmarks_file_save(priv->bookmarks_list, priv->bookmarks_file);
        g_free(bookmars_file);

        g_free(priv->hhc);
        g_free(priv->hhk);
        g_free(priv->bookfolder);
        g_free(priv->filename);

        g_free(priv->bookname);
        g_free(priv->homepage);
        g_free(priv->encoding);
        g_free(priv->variable_font);
        g_free(priv->fixed_font);

        if (priv->toc_tree) {
                g_node_destroy(priv->toc_tree);
        }

        G_OBJECT_CLASS (cs_chmfile_parent_class)->finalize (object);
}

/* Internal functions */

static int
dir_exists(const char *path)
{
        struct stat statbuf;

        return stat(path, &statbuf) != -1 ? 1 : 0;
}

static int
rmkdir(char *path)
{
        /*
         * strip off trailing components unless we can stat the directory, or we
         * have run out of components
         */

        char *i = rindex(path, '/');

        if (path[0] == '\0'  ||  dir_exists(path))
                return 0;

        if (i != NULL) {
                *i = '\0';
                rmkdir(path);
                *i = '/';
                mkdir(path, 0777);
        }

        return dir_exists(path) ? 0 : -1;
}

/*
 * callback function for enumerate API
 */
static int
_extract_callback(struct chmFile *h, struct chmUnitInfo *ui, void *context)
{
        gchar *fname = NULL;
        LONGUINT64 ui_path_len;
        char buffer[32768];
        char *i;

        struct extract_context *ctx = (struct extract_context *)context;

        if (ui->path[0] != '/') {
                return CHM_ENUMERATOR_CONTINUE;
        }

        if (strstr(ui->path, "/../") != NULL) {
                return CHM_ENUMERATOR_CONTINUE;
        }

        if (snprintf(buffer, sizeof(buffer), "%s%s", ctx->base_path, ui->path) > 1024) {
                return CHM_ENUMERATOR_FAILURE;
        }

        g_debug("CS_CHMFILE: ui->path = %s", ui->path);

        fname = g_build_filename(ctx->base_path, ui->path+1, NULL);

        ui_path_len = strlen(ui->path)-1;

        /* Distinguish between files and dirs */
        if (ui->path[ui_path_len] != '/' ) {
                FILE *fout;
                LONGINT64 len, remain = ui->length;
                LONGUINT64 offset = 0;

                gchar *file_ext;

                file_ext = g_strrstr(g_path_get_basename(ui->path), ".");
                g_debug("file_ext = %s", file_ext);

                if ((fout = fopen(fname, "wb")) == NULL) {
                        /* make sure that it isn't just a missing directory before we abort */
                        char newbuf[32768];
                        strcpy(newbuf, fname);
                        i = rindex(newbuf, '/');
                        *i = '\0';
                        rmkdir(newbuf);

                        if ((fout = fopen(fname, "wb")) == NULL) {
                                g_message("CHM_ENUMERATOR_FAILURE fopen");
                                return CHM_ENUMERATOR_FAILURE;
                        }
                }

                while (remain != 0) {
                        len = chm_retrieve_object(h, ui, (unsigned char *)buffer, offset, 32768);
                        if (len > 0) {
                                if(fwrite(buffer, 1, (size_t)len, fout) != len) {
                                        g_message("CHM_ENUMERATOR_FAILURE fwrite");
                                        return CHM_ENUMERATOR_FAILURE;
                                }
                                offset += len;
                                remain -= len;
                        } else {
                                break;
                        }
                }

                fclose(fout);
                extract_post_file_write(fname);
        } else {
                if (rmkdir(fname) == -1) {
                        g_message("CHM_ENUMERATOR_FAILURE rmkdir");
                        return CHM_ENUMERATOR_FAILURE;
                }
        }
        g_free(fname);

        return CHM_ENUMERATOR_CONTINUE;
}

static gboolean
extract_chm(const gchar *filename, const gchar *base_path)
{
        struct chmFile *handle;
        struct extract_context ec;

        handle = chm_open(filename);

        if (handle == NULL) {
                g_message(_("cannot open chmfile: %s"), filename);
                return FALSE;
        }

        ec.base_path = base_path

        if (!chm_enumerate(handle,
                           CHM_ENUMERATE_NORMAL | CHM_ENUMERATE_SPECIAL,
                           _extract_callback,
                           (void *)&ec)) {
                g_message(_("Extract chmfile failed: %s"), filename);
                return FALSE;
        }

        chm_close(handle);

        return TRUE;
}

static char *
MD5File(const char *filename, char *buf)
{
        unsigned char buffer[1024];
        unsigned char* digest;
        static const char hex[]="0123456789abcdef";

        gcry_md_hd_t hd;
        int f, i;

        if(filename == NULL)
                return NULL;

        gcry_md_open(&hd, GCRY_MD_MD5, 0);
        f = open(filename, O_RDONLY);
        if (f < 0) {
                g_message(_("open \"%s\" failed: %s"), filename, strerror(errno));
                return NULL;
        }

        while ((i = read(f, buffer, sizeof(buffer))) > 0)
                gcry_md_write(hd, buffer, i);

        close(f);

        if (i < 0)
                return NULL;

        if (buf == NULL)
                buf = malloc(33);
        if (buf == NULL)
                return (NULL);

        digest = (unsigned char*)gcry_md_read(hd, 0);

        for (i = 0; i < 16; i++) {
                buf[i+i] = hex[(u_int32_t)digest[i] >> 4];
                buf[i+i+1] = hex[digest[i] & 0x0f];
        }

        buf[i+i] = '\0';
        return (buf);
}

static u_int32_t
get_dword(const unsigned char *buf)
{
        u_int32_t result;

        result = buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);

        if (result == 0xFFFFFFFF)
                result = 0;

        return result;
}

static void
chmfile_file_info(CsChmfile *self)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);

        struct chmFile    *cfd = chm_open(priv->filename);

        if (cfd == NULL) {
                g_error(_("Can not open chm file %s."), priv->filename);
                return;
        }

        chmfile_system_info(cfd, chmfile);
        chmfile_windows_info(cfd, chmfile);

        /* convert bookname to UTF-8 */
        if (priv->bookname != NULL && priv->encoding != NULL) {
                gchar *bookname_utf8;

                bookname_utf8 = g_convert(priv->bookname, -1, "UTF-8",
                                       priv->encoding,
                                       NULL, NULL, NULL);
                g_free(priv->bookname);
                priv->bookname = bookname_utf8;
        }

        /* convert filename to UTF-8 */
        if (priv->hhc != NULL && priv->encoding != NULL) {
                gchar *filename_utf8;

                filename_utf8 = convert_filename_to_utf8(priv->hhc, priv->encoding);
                g_free(priv->hhc);
                priv->hhc = filename_utf8;
        }

        if (priv->hhk != NULL && priv->encoding != NULL) {
                gchar *filename_utf8;

                filename_utf8 = convert_filename_to_utf8(priv->hhk, priv->encoding);
                g_free(priv->hhk);
                priv->hhk = filename_utf8;
        }

        chm_close(cfd);
}

static void
chmfile_windows_info(struct chmFile *cfd, CsChmfile *self)
{
        struct chmUnitInfo ui;
        unsigned char buffer[4096];
        size_t size = 0;
        u_int32_t entries, entry_size;
        u_int32_t hhc, hhk, bookname, homepage;

        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);

        if (chm_resolve_object(cfd, "/#WINDOWS", &ui) != CHM_RESOLVE_SUCCESS)
                return;

        size = chm_retrieve_object(cfd, &ui, buffer, 0L, 8);

        if (size < 8)
                return;

        entries = get_dword(buffer);
        if (entries < 1)
                return;

        entry_size = get_dword(buffer + 4);
        size = chm_retrieve_object(cfd, &ui, buffer, 8L, entry_size);
        if (size < entry_size)
                return;

        hhc = get_dword(buffer + 0x60);
        hhk = get_dword(buffer + 0x64);
        homepage = get_dword(buffer + 0x68);
        bookname = get_dword(buffer + 0x14);

        if (chm_resolve_object(cfd, "/#STRINGS", &ui) != CHM_RESOLVE_SUCCESS)
                return;

        size = chm_retrieve_object(cfd, &ui, buffer, 0L, 4096);

        if (!size)
                return;

        if (priv->hhc == NULL && hhc)
                priv->hhc = g_strdup_printf("/%s", buffer + hhc);
        if (priv->hhk == NULL && hhk)
                priv->hhk = g_strdup_printf("/%s", buffer + hhk);
        if (priv->homepage == NULL && homepage)
                priv->homepage = g_strdup_printf("/%s", buffer + homepage);

        if (priv->bookname == NULL) {
                if (bookname)
                        priv->bookname = g_strdup((char *)buffer + bookname);
                else
                        priv->bookname = g_strdup(g_path_get_basename(priv->filename));
        }
}

static void
chmfile_system_info(struct chmFile *cfd, CsChmfile *self)
{
        struct chmUnitInfo ui;
        unsigned char buffer[4096];

        int index = 0;
        unsigned char* cursor = NULL;
        u_int16_t value = 0;
        u_int32_t lcid = 0;
        size_t size = 0;

        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);

        if (chm_resolve_object(cfd, "/#SYSTEM", &ui) != CHM_RESOLVE_SUCCESS)
                return;

        size = chm_retrieve_object(cfd, &ui, buffer, 4L, 4096);

        if (!size)
                return;

        buffer[size - 1] = 0;

        for(;;) {
                // This condition won't hold if I process anything
                // except NUL-terminated strings!
                if(index > size - 1 - (long)sizeof(u_int16_t))
                        break;

                cursor = buffer + index;
                value = UINT16ARRAY(cursor);
                g_debug("system value = %d", value);
                switch(value) {
                case 0:
                        index += 2;
                        cursor = buffer + index;

                        priv->hhc = g_strdup_printf("/%s", buffer + index + 2);
                        g_debug("CS_CHMFILE: hhc %s", priv->hhc);

                        break;
                case 1:
                        index += 2;
                        cursor = buffer + index;

                        priv->hhk = g_strdup_printf("/%s", buffer + index + 2);
                        g_debug("CS_CHMFILE: hhk %s", priv->hhk);

                        break;
                case 2:
                        index += 2;
                        cursor = buffer + index;

                        priv->homepage = g_strdup_printf("/%s", buffer + index + 2);
                        g_debug("CS_CHMFILE: homepage %s", priv->homepage);

                        break;
                case 3:
                        index += 2;
                        cursor = buffer + index;

                        priv->bookname = g_strdup((char *)buffer + index + 2);
                        g_debug("CS_CHMFILE: bookname %s", priv->bookname);

                        break;
                case 4: // LCID stuff
                        index += 2;
                        cursor = buffer + index;

                        lcid = UINT32ARRAY(buffer + index + 2);
                        g_debug("lcid %x", lcid);
                        chmfile_set_encoding(chmfile, get_encoding_by_lcid(lcid));
                        break;

                case 6:
                        index += 2;
                        cursor = buffer + index;

                        if(!priv->hhc) {
                                char *hhc, *hhk;

                                hhc = g_strdup_printf("/%s.hhc", buffer + index + 2);
                                hhk = g_strdup_printf("/%s.hhk", buffer + index + 2);

                                if (chm_resolve_object(cfd, hhc, &ui) == CHM_RESOLVE_SUCCESS)
                                        priv->hhc = hhc;

                                if (chm_resolve_object(cfd, hhk, &ui) == CHM_RESOLVE_SUCCESS)
                                        priv->hhk = hhk;
                        }

                        break;
                case 16:
                        index += 2;
                        cursor = buffer + index;

                        g_debug("CS_CHMFILE: font %s", buffer + index + 2);
                        break;

                default:
                        index += 2;
                        cursor = buffer + index;
                }

                value = UINT16ARRAY(cursor);
                index += value + 2;
        }
}

static GNode *
parse_hhc_file(const gchar *file, const gchar *encoding)
{
        return cs_parse_file(file, encoding);
}

static GList *
parse_hhk_file(const gchar *file, const gchar *encoding)
{
        GNode *tree;
        GList *list;
        
        tree = cs_parse_file(file, encoding);
        /* convert GNode to GList */ // FIXME

        return list;
}

static void
load_bookinfo(CsChmfile *self)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);

        gchar *bookinfo_file = g_build_filename(priv->dir, CHMSEE_BOOKINFO_FILE, NULL);

        g_debug("CS_CHMFILE: read bookinfo file = %s", bookinfo_file);

        GError     *error = NULL;
        GKeyFile *keyfile = g_key_file_new();

        gboolean rv = g_key_file_load_from_file(keyfile, bookinfo_file, G_KEY_FILE_NONE, NULL);

        if (rv) {
                priv->hhc          = g_key_file_get_string(keyfile, "Bookinfo", "hhc", &error);
                priv->hhk          = g_key_file_get_string(keyfile, "Bookinfo", "hhk", &error);
                priv->homepage     = g_key_file_get_string(keyfile, "Bookinfo", "homepage", &error);
                priv->bookname     = g_key_file_get_string(keyfile, "Bookinfo", "bookname", &error);
                priv->encoding     = g_key_file_get_string(keyfile, "Bookinfo", "encoding", &error);
                priv->varible_font = g_key_file_get_string(keyfile, "Bookinfo", "varible_font", &error);
                priv->fixed_font   = g_key_file_get_string(keyfile, "Bookinfo", "fixed_font", &error);
        }

        g_key_file_free(keyfile);
        g_free(bookinfo_file);
}

static void
save_bookinfo(CsChmfile *self)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);

        gchar *bookinfo_file = g_build_filename(priv->bookfolder, CHMSEE_BOOKINFO_FILE, NULL);

        g_debug("CS_CHMFILE: save bookinfo file = %s", bookinfo_file);

        GKeyFile *keyfile = g_key_file_new();

        g_key_file_set_string(keyfile, "Bookinfo", "hhc", priv->hhc);
        g_key_file_set_string(keyfile, "Bookinfo", "hhk", priv->hhk);
        g_key_file_set_string(keyfile, "Bookinfo", "homepage", priv->homepage);
        g_key_file_set_string(keyfile, "Bookinfo", "bookname", priv->bookname);
        g_key_file_set_string(keyfile, "Bookinfo", "encoding", priv->encoding);
        g_key_file_set_string(keyfile, "Bookinfo", "variable_font", priv->varible_font);
        g_key_file_set_string(keyfile, "Bookinfo", "fixed_font", priv->fixed_font);

        gsize    length = 0;
        GError   *error = NULL;
        gchar *contents = g_key_file_to_data(keyfile, &length, &error);
        g_file_set_contents(bookinfo_file, contents, length, &error);

        g_key_file_free(keyfile);
        g_free(contents);
        g_free(bookinfo_file);
}

/* External functions */

CsChmfile *
cs_chmfile_new(const gchar *filename, const gchar *bookshelf)
{
        CsChmfile        *self = g_object_new(CS_TYPE_CHMFILE, NULL);
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);

        /* use chm file's MD5 as folder name */
        gchar *md5 = MD5File(filename, NULL);
        if(!md5) {
                g_warning("CS_CHMFILE: Oops, cannot calculate chmfile's MD5 value.");
                return NULL;
        }

        priv->filename = g_strdup(filename);
        priv->bookfolder = g_build_filename(bookshelf, md5, NULL);
        g_debug("CS_CHMFILE: book folder = %s", priv->bookfolder);
        g_debug("CS_CHMFILE: filename = %s", priv->filename);

        /* if this chmfile already exists in bookshelf, load it's bookinfo */
        if (g_file_test(priv->bookfolder, G_FILE_TEST_IS_DIR)) {
                load_bookinfo(self);
        } else {
                if (!extract_chm(filename, self)) {
                        g_warning("CS_CHMFILE: extract_chm failed: %s", filename);
                        return NULL;
                }

                chmfile_file_info(self);
                save_bookinfo(self);
        }

        g_debug("CS_CHMFILE: priv->hhc = %s", priv->hhc);
        g_debug("CS_CHMFILE: priv->hhk = %s", priv->hhk);
        g_debug("CS_CHMFILE: priv->homepage = %s", priv->homepage);
        g_debug("CS_CHMFILE: priv->bookname = %s", priv->bookname);
        g_debug("CS_CHMFILE: priv->endcoding = %s", priv->encoding);

        /* parse hhc file */
        if (priv->hhc != NULL && g_ascii_strcasecmp(priv->hhc, "(null)") != 0) {
                gchar *hhcfile = g_build_filename(priv->dir, priv->hhc, NULL);

                if (!g_file_test(hhcfile, G_FILE_TEST_EXISTS)) {
                        hhcfile = file_exist_ncase(hhcfile); // FIXME: g_free(hhcfile)
                }

                priv->toc_tree = parse_hhc(hhcfile, priv->encoding);
                g_free(hhcfile);
        }

        /* parse hhk file */
        if (priv->hhk != NULL && priv->index == NULL) {
                gchar* path = g_build_filename(priv->dir, priv->hhk, NULL);
                gchar* path2 = correct_filename(path);

                priv->index_list = parse_hhk(path2, priv->encoding);

                g_free(path);
                g_free(path2);
        }

        /* Load bookmarks */
        gchar *bookmarks_file = g_build_filename(priv->bookfolder, CHMSEE_BOOKMARKS_FILE, NULL);
        priv->bookmarks_list = cs_bookmarks_file_load(bookmarks_file);

        g_free(bookmarks_file);
        g_free(md5);

        return self;
}

/* see http://code.google.com/p/chmsee/issues/detail?id=12 */
void extract_post_file_write(const gchar* fname) {
        gchar* basename = g_path_get_basename(fname);
        gchar* pos = strchr(basename, ';');
        if(pos) {
                gchar* dirname = g_path_get_dirname(fname);
                *pos = '\0';
                gchar* newfname = g_build_filename(dirname, basename, NULL);
                if(g_rename(fname, newfname) != 0) {
                        g_error("rename \"%s\" to \"%s\" failed: %s", fname, newfname, strerror(errno));
                }
                g_free(dirname);
                g_free(newfname);
        }
        g_free(basename);
}

static const gchar *
chmfile_get_dir(CsChmfile* self)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);
        return priv->dir;
}

static const gchar *
chmfile_get_fixed_font(CsChmfile* self)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);
        return priv->fixed_font;
}

static const gchar *
chmfile_get_variable_font(CsChmfile* self)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);
        return priv->variable_font;
}

static GNode *
chmfile_get_toc_tree(CsChmfile* self)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);
        return priv->toc_tree;
}

static Bookmarks *
chmfile_get_bookmarks_list(CsChmfile* self)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);
        return priv->bookmarks_list;
}

static void
chmfile_set_fixed_font(CsChmfile* self, const gchar* font)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);
        g_free(priv->fixed_font);
        priv->fixed_font = g_strdup(font);
}

static void
chmfile_set_variable_font(CsChmfile* self, const gchar* font)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);
        g_free(priv->variable_font);
        priv->variable_font = g_strdup(font);
}

void
chmfile_set_encoding(CsChmfile* self, const char* encoding)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);
        if(priv->encoding) {
                g_free(priv->encoding);
        }
        priv->encoding = g_strdup(encoding);
}

const gchar *
cs_chmfile_get_filename(CsChmfile *self)
{
        return CS_CHMFILE_GET_PRIVATE (self)->filename;
}

const gchar *
cs_chmfile_get_bookname(CsChmfile* self)
{
        return CS_CHMFILE_GET_PRIVATE (self)->bookname;
}

const gchar *
cs_chmfile_get_homepage(CsChmfile *self)
{
        return CS_CHMFILE_GET_PRIVATE (self)->homepage;
}

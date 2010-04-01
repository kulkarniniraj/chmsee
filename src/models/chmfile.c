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
#include <fcntl.h>
#include <sys/stat.h>
#include <gcrypt.h>
#include <chm_lib.h>

#include "chmfile.h"
#include "utils.h"
#include "models/bookmarksfile.h"
#include "models/parser.h"
#include "models/link.h"

typedef struct _CsChmfilePrivate CsChmfilePrivate;

struct _CsChmfilePrivate {
        GNode       *toc_tree;
        GList       *index_list;
        GList       *bookmarks_list;

        gchar       *hhc;
        gchar       *hhk;
        gchar       *bookfolder;     /* the folder CHM file extracted to */
        gchar       *filename;       /* opened CHM file name */

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
static void cs_chmfile_finalize(GObject *);

static void chmfile_file_info(CsChmfile *);
static void chmfile_system_info(struct chmFile *, CsChmfile *);
static void chmfile_windows_info(struct chmFile *, CsChmfile *);

static int dir_exists(const char *);
static int rmkdir(char *);
static int _extract_callback(struct chmFile *, struct chmUnitInfo *, void *);
static const char *get_encoding_by_lcid(guint32);

static gboolean extract_chm(const gchar *, const gchar *);
static void load_bookinfo(CsChmfile *);
static void save_bookinfo(CsChmfile *);
static void extract_post_file_write(const gchar *);

static GNode *parse_hhc_file(const gchar *, const gchar*);
static void parse_hhk_file(CsChmfile *, const gchar *, const gchar*);
static void tree_to_list_callback(GNode *, gpointer);
static void free_list_data(gpointer, gpointer);
static gchar *check_file_ncase(CsChmfile *, gchar *);

/* GObject functions */

G_DEFINE_TYPE (CsChmfile, cs_chmfile, G_TYPE_OBJECT);

static void
cs_chmfile_class_init(CsChmfileClass *klass)
{
        g_type_class_add_private(klass, sizeof(CsChmfilePrivate));

        GObjectClass *object_class = G_OBJECT_CLASS(klass);

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
cs_chmfile_finalize(GObject *object)
{
        g_debug("CS_CHMFILE >>> finalize");

        CsChmfile        *self = CS_CHMFILE (object);
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);

        save_bookinfo(self);

        g_free(priv->hhc);
        g_free(priv->hhk);
        g_free(priv->bookfolder);
        g_free(priv->filename);

        g_free(priv->bookname);
        g_free(priv->homepage);
        g_free(priv->encoding);
        g_free(priv->variable_font);
        g_free(priv->fixed_font);

        if (priv->index_list) {
                g_list_foreach(priv->index_list, (GFunc)free_list_data, NULL);
                g_list_free(priv->index_list);

                priv->index_list = NULL;
        }

        if (priv->toc_tree) {
                g_node_destroy(priv->toc_tree);
                priv->toc_tree = NULL;
        }
        priv->bookmarks_list = NULL;

        g_debug("CS_CHMFILE >>> finalized");
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

        fname = g_build_filename(ctx->base_path, ui->path+1, NULL);

        ui_path_len = strlen(ui->path)-1;

        /* Distinguish between files and dirs */
        if (ui->path[ui_path_len] != '/' ) {
                FILE *fout;
                LONGINT64 len, remain = ui->length;
                LONGUINT64 offset = 0;

                if ((fout = fopen(fname, "wb")) == NULL) {
                        /* make sure that it isn't just a missing directory before we abort */
                        char newbuf[32768];
                        strcpy(newbuf, fname);
                        i = rindex(newbuf, '/');
                        *i = '\0';
                        rmkdir(newbuf);

                        if ((fout = fopen(fname, "wb")) == NULL) {
                                g_debug("CS_CHMFILE: CHM_ENUMERATOR_FAILURE fopen");
                                g_free(fname);
                                return CHM_ENUMERATOR_FAILURE;
                        }
                }

                while (remain != 0) {
                        len = chm_retrieve_object(h, ui, (unsigned char *)buffer, offset, 32768);
                        if (len > 0) {
                                if(fwrite(buffer, 1, (size_t)len, fout) != len) {
                                        g_debug("CS_CHMFILE: CHM_ENUMERATOR_FAILURE fwrite");
                                        g_free(fname);
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
                        g_debug("CS_CHMFILE >>> CHM_ENUMERATOR_FAILURE rmkdir");
                        g_free(fname);
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
                g_warning(_("Cannot open chmfile: %s"), filename);
                return FALSE;
        }

        ec.base_path = base_path;

        if (!chm_enumerate(handle,
                           CHM_ENUMERATE_NORMAL | CHM_ENUMERATE_SPECIAL,
                           _extract_callback,
                           (void *)&ec)) {
                g_warning(_("Extract chmfile failed: %s"), filename);
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
                g_warning(_("Open \"%s\" failed: %s"), filename, strerror(errno));
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

static const char *
get_encoding_by_lcid(guint32 lcid)
{
        switch(lcid) {
        case 0x0436:
        case 0x042d:
        case 0x0403:
        case 0x0406:
        case 0x0413:
        case 0x0813:
        case 0x0409:
        case 0x0809:
        case 0x0c09:
        case 0x1009:
        case 0x1409:
        case 0x1809:
        case 0x1c09:
        case 0x2009:
        case 0x2409:
        case 0x2809:
        case 0x2c09:
        case 0x3009:
        case 0x3409:
        case 0x0438:
        case 0x040b:
        case 0x040c:
        case 0x080c:
        case 0x0c0c:
        case 0x100c:
        case 0x140c:
        case 0x180c:
        case 0x0407:
        case 0x0807:
        case 0x0c07:
        case 0x1007:
        case 0x1407:
        case 0x040f:
        case 0x0421:
        case 0x0410:
        case 0x0810:
        case 0x043e:
        case 0x0414:
        case 0x0814:
        case 0x0416:
        case 0x0816:
        case 0x040a:
        case 0x080a:
        case 0x0c0a:
        case 0x100a:
        case 0x140a:
        case 0x180a:
        case 0x1c0a:
        case 0x200a:
        case 0x240a:
        case 0x280a:
        case 0x2c0a:
        case 0x300a:
        case 0x340a:
        case 0x380a:
        case 0x3c0a:
        case 0x400a:
        case 0x440a:
        case 0x480a:
        case 0x4c0a:
        case 0x500a:
        case 0x0441:
        case 0x041d:
        case 0x081d:
                return "ISO-8859-1";
                break;
        case 0x041c:
        case 0x041a:
        case 0x0405:
        case 0x040e:
        case 0x0418:
        case 0x0815:
        case 0x081a:
        case 0x041b:
        case 0x0424:
                return "ISO-8859-2";
                break;
        case 0x0c01:
                return "WINDOWS-1256";
        case 0x0401:
        case 0x0801:
        case 0x1001:
        case 0x1401:
        case 0x1801:
        case 0x1c01:
        case 0x2001:
        case 0x2401:
        case 0x2801:
        case 0x2c01:
        case 0x3001:
        case 0x3401:
        case 0x3801:
        case 0x3c01:
        case 0x4001:
        case 0x0429:
        case 0x0420:
                return "ISO-8859-6";
                break;
        case 0x0408:
                return "ISO-8859-7";
                break;
        case 0x040d:
                return "ISO-8859-8";
                break;
        case 0x042c:
        case 0x041f:
        case 0x0443:
                return "ISO-8859-9";
                break;
        case 0x041e:
                return "ISO-8859-11";
                break;
        case 0x0425:
        case 0x0426:
        case 0x0427:
                return "ISO-8859-13";
                break;
        case 0x0411:
                return "cp932";
                break;
        case 0x0804:
        case 0x1004:
                return "cp936";
                break;
        case 0x0412:
                return "cp949";
                break;
        case 0x0404:
        case 0x0c04:
        case 0x1404:
                return "cp950";
                break;
        case 0x082c:
        case 0x0423:
        case 0x0402:
        case 0x043f:
        case 0x042f:
        case 0x0419:
        case 0x0c1a:
        case 0x0444:
        case 0x0422:
        case 0x0843:
                return "cp1251";
                break;
        default:
                return "";
                break;
        }
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

        struct chmFile *cfd = chm_open(priv->filename);

        if (cfd == NULL) {
                g_error(_("Can not open chm file %s."), priv->filename);
                return;
        }

        chmfile_system_info(cfd, self);
        chmfile_windows_info(cfd, self);

        if (priv->hhc != NULL) {
                check_file_ncase(self, priv->hhc);
        }

        if (priv->hhk != NULL) {
                check_file_ncase(self, priv->hhk);
        }

        /* convert encoding to UTF-8 */
        if (priv->encoding != NULL) {
                if (priv->bookname) {
                        gchar *bookname_utf8 = g_convert(priv->bookname, -1, "UTF-8",
                                                         priv->encoding,
                                                         NULL, NULL, NULL);
                        g_debug("CS_CHMFILE >>> priv->bookname = %s, bookname_utf8 = %s", priv->bookname, bookname_utf8);
                        g_free(priv->bookname);
                        priv->bookname = bookname_utf8;
                }

                if (priv->hhc) {
                        gchar *hhc_utf8 = convert_filename_to_utf8(priv->hhc, priv->encoding);
                        g_free(priv->hhc);
                        priv->hhc = hhc_utf8;
                }

                if (priv->hhk) {
                        gchar *hhk_utf8 = convert_filename_to_utf8(priv->hhk, priv->encoding);
                        g_free(priv->hhk);
                        priv->hhk = hhk_utf8;
                }
        }

        if (priv->bookname == NULL)
                priv->bookname = g_path_get_basename(priv->filename);

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

        if (priv->bookname == NULL && bookname)
                priv->bookname = g_strdup((char *)buffer + bookname);
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
                g_debug("CS_CHMFILE >>> system value = %d", value);
                switch(value) {
                case 0:
                        index += 2;
                        cursor = buffer + index;

                        priv->hhc = g_strdup_printf("/%s", buffer + index + 2);
                        g_debug("CS_CHMFILE >>> hhc %s", priv->hhc);

                        break;
                case 1:
                        index += 2;
                        cursor = buffer + index;

                        priv->hhk = g_strdup_printf("/%s", buffer + index + 2);
                        g_debug("CS_CHMFILE >>> hhk %s", priv->hhk);

                        break;
                case 2:
                        index += 2;
                        cursor = buffer + index;

                        priv->homepage = g_strdup_printf("/%s", buffer + index + 2);
                        g_debug("CS_CHMFILE >>> homepage %s", priv->homepage);

                        break;
                case 3:
                        index += 2;
                        cursor = buffer + index;

                        priv->bookname = g_strdup((char *)buffer + index + 2);
                        g_debug("CS_CHMFILE >>> bookname %s", priv->bookname);

                        break;
                case 4: // LCID stuff
                        index += 2;
                        cursor = buffer + index;

                        lcid = UINT32ARRAY(buffer + index + 2);
                        g_debug("CS_CHMFILE >>> lcid %x", lcid);

                        if(priv->encoding)
                                g_free(priv->encoding);

                        priv->encoding = g_strdup(get_encoding_by_lcid(lcid));
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

                        g_debug("CS_CHMFILE >>> font %s", buffer + index + 2);
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
        g_debug("CS_CHMFILE >>> parse hhc file = %s, encoding = %s", file, encoding);
        return cs_parse_file(file, encoding);
}

static void
parse_hhk_file(CsChmfile *self, const gchar *file, const gchar *encoding)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);

        g_debug("CHMFILE >>> parse hhk file = %s, encoding = %s", file, encoding);

        GNode *tree = cs_parse_file(file, encoding);
        g_debug("CHMFILE >>> parse hhk file tree = %p", tree);

        /* convert GNode to GList */
        g_node_children_foreach(tree, G_TRAVERSE_LEAVES, tree_to_list_callback, priv);
        g_node_destroy(tree);
}

static void
tree_to_list_callback(GNode *node, gpointer data)
{
        CsChmfilePrivate *priv = (CsChmfilePrivate *)data;
        priv->index_list = g_list_append(priv->index_list, node->data);
}

static void
free_list_data(gpointer data, gpointer user_data)
{
        Link *link = (Link *)data;
        link_free(link);
}

static void
load_bookinfo(CsChmfile *self)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);

        gchar *bookinfo_file = g_build_filename(priv->bookfolder, CHMSEE_BOOKINFO_FILE, NULL);

        g_debug("CS_CHMFILE >>> read bookinfo file = %s", bookinfo_file);

        GKeyFile *keyfile = g_key_file_new();

        gboolean rv = g_key_file_load_from_file(keyfile, bookinfo_file, G_KEY_FILE_NONE, NULL);

        if (!rv) {
                convert_old_config_file(bookinfo_file, "[Bookinfo]\n");
        }

        rv = g_key_file_load_from_file(keyfile, bookinfo_file, G_KEY_FILE_NONE, NULL);

        if (rv) {
                priv->hhc           = g_key_file_get_string(keyfile, "Bookinfo", "hhc", NULL);
                priv->hhk           = g_key_file_get_string(keyfile, "Bookinfo", "hhk", NULL);
                priv->homepage      = g_key_file_get_string(keyfile, "Bookinfo", "homepage", NULL);
                priv->bookname      = g_key_file_get_string(keyfile, "Bookinfo", "bookname", NULL);
                priv->encoding      = g_key_file_get_string(keyfile, "Bookinfo", "encoding", NULL);
                priv->variable_font = g_key_file_get_string(keyfile, "Bookinfo", "variable_font", NULL);
                priv->fixed_font    = g_key_file_get_string(keyfile, "Bookinfo", "fixed_font", NULL);
        }

        g_key_file_free(keyfile);
        g_free(bookinfo_file);
}

static void
save_bookinfo(CsChmfile *self)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);

        gchar *bookinfo_file = g_build_filename(priv->bookfolder, CHMSEE_BOOKINFO_FILE, NULL);

        g_debug("CS_CHMFILE >>> save bookinfo file = %s", bookinfo_file);

        GKeyFile *keyfile = g_key_file_new();

        if (priv->hhc)
                g_key_file_set_string(keyfile, "Bookinfo", "hhc", priv->hhc);
        if (priv->hhk)
                g_key_file_set_string(keyfile, "Bookinfo", "hhk", priv->hhk);
        if (priv->homepage)
                g_key_file_set_string(keyfile, "Bookinfo", "homepage", priv->homepage);
        if (priv->bookname)
                g_key_file_set_string(keyfile, "Bookinfo", "bookname", priv->bookname);
        g_key_file_set_string(keyfile, "Bookinfo", "encoding", priv->encoding);
        g_key_file_set_string(keyfile, "Bookinfo", "variable_font", priv->variable_font);
        g_key_file_set_string(keyfile, "Bookinfo", "fixed_font", priv->fixed_font);

        gsize    length = 0;
        gchar *contents = g_key_file_to_data(keyfile, &length, NULL);
        g_file_set_contents(bookinfo_file, contents, length, NULL);

        g_key_file_free(keyfile);
        g_free(contents);
        g_free(bookinfo_file);
}

static gchar *
check_file_ncase(CsChmfile *self, gchar *path)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);
        gchar *filename = g_build_filename(priv->bookfolder, path, NULL);

        if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
                g_free(path);
                path = NULL;

                gchar *found = file_exist_ncase(filename);
                if (found) {
                        path = g_path_get_basename(found);
                        g_free(found);
                }
        }
        g_free(filename);
        return path;
}

/* see http://code.google.com/p/chmsee/issues/detail?id=12 */
static void
extract_post_file_write(const gchar *fname)
{
        gchar *basename = g_path_get_basename(fname);
        gchar *pos = strchr(basename, ';');
        if (pos) {
                gchar *dirname = g_path_get_dirname(fname);
                *pos = '\0';
                gchar *newfname = g_build_filename(dirname, basename, NULL);
                if (rename(fname, newfname) != 0) {
                        g_error("rename \"%s\" to \"%s\" failed: %s", fname, newfname, strerror(errno));
                }
                g_free(dirname);
                g_free(newfname);
        }
        g_free(basename);
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
                g_warning("CS_CHMFILE >>> Oops!! cannot calculate chmfile's MD5 value.");
                return NULL;
        }

        priv->filename = g_strdup(filename);
        priv->bookfolder = g_build_filename(bookshelf, md5, NULL);
        g_debug("CS_CHMFILE >>> book folder = %s", priv->bookfolder);

        /* if this chmfile already exists in bookshelf, load it's bookinfo */
        if (g_file_test(priv->bookfolder, G_FILE_TEST_IS_DIR)) {
                load_bookinfo(self);
        } else {
                if (!extract_chm(filename, priv->bookfolder)) {
                        g_warning("CS_CHMFILE >>> extract_chm failed: %s", filename);
                        return NULL;
                }

                chmfile_file_info(self);
                save_bookinfo(self);
        }

        g_debug("CS_CHMFILE >>> priv->hhc = %s", priv->hhc);
        g_debug("CS_CHMFILE >>> priv->hhk = %s", priv->hhk);
        g_debug("CS_CHMFILE >>> priv->hhk p= %p", priv->hhk);
        g_debug("CS_CHMFILE >>> priv->homepage = %s", priv->homepage);
        g_debug("CS_CHMFILE >>> priv->bookname = %s", priv->bookname);
        g_debug("CS_CHMFILE >>> priv->endcoding = %s", priv->encoding);

        /* parse hhc file */
        if (priv->hhc != NULL && g_ascii_strcasecmp(priv->hhc, "(null)") != 0) {
                gchar *hhcfile = g_build_filename(priv->bookfolder, priv->hhc, NULL);

                priv->toc_tree = parse_hhc_file(hhcfile, priv->encoding);
                g_free(hhcfile);
        }

        /* parse hhk file */
        if (priv->hhk != NULL && priv->index_list == NULL) {
                gchar *path = g_build_filename(priv->bookfolder, priv->hhk, NULL);

                parse_hhk_file(self, path, priv->encoding);
                g_debug("CS_CHMFILE >>> priv->index_list = %p", priv->index_list);
                g_free(path);
        }

        /* Load bookmarks */
        gchar *bookmarks_file = g_build_filename(priv->bookfolder, CHMSEE_BOOKMARKS_FILE, NULL);
        priv->bookmarks_list = cs_bookmarks_file_load(bookmarks_file);

        g_free(bookmarks_file);
        g_free(md5);

        return self;
}

GNode *
cs_chmfile_get_toc_tree(CsChmfile *self)
{
        return CS_CHMFILE_GET_PRIVATE (self)->toc_tree;
}

GList *
cs_chmfile_get_index_list(CsChmfile *self)
{
        return CS_CHMFILE_GET_PRIVATE (self)->index_list;
}

GList *
cs_chmfile_get_bookmarks_list(CsChmfile *self)
{
        return CS_CHMFILE_GET_PRIVATE (self)->bookmarks_list;
}

void
cs_chmfile_update_bookmarks_list(CsChmfile *self, GList *links)
{
        g_debug("CS_CHMFILE >>> update bookmarks bookmarks_list");

        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);
        priv->bookmarks_list = links;

        gchar *bookmarks_file = g_build_filename(priv->bookfolder, CHMSEE_BOOKMARKS_FILE, NULL);
        cs_bookmarks_file_save(priv->bookmarks_list, bookmarks_file);

        g_free(bookmarks_file);
}


const gchar *
cs_chmfile_get_filename(CsChmfile *self)
{
        return CS_CHMFILE_GET_PRIVATE (self)->filename;
}

const gchar *
cs_chmfile_get_bookfolder(CsChmfile *self)
{
        return CS_CHMFILE_GET_PRIVATE (self)->bookfolder;
}

const gchar *
cs_chmfile_get_bookname(CsChmfile *self)
{
        return CS_CHMFILE_GET_PRIVATE (self)->bookname;
}

const gchar *
cs_chmfile_get_homepage(CsChmfile *self)
{
        g_return_val_if_fail(IS_CS_CHMFILE (self), NULL);

        return CS_CHMFILE_GET_PRIVATE (self)->homepage;
}

const gchar *
cs_chmfile_get_variable_font(CsChmfile *self)
{
        g_debug("CS_CHMFILE >>> get variable font");
        return CS_CHMFILE_GET_PRIVATE (self)->variable_font;
}

void
cs_chmfile_set_variable_font(CsChmfile *self, const gchar *font_name)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);

        g_free(priv->variable_font);

        priv->variable_font = g_strdup(font_name);
}

const gchar *
cs_chmfile_get_fixed_font(CsChmfile *self)
{
        g_debug("CS_CHMFILE >>> get fixed font");
        return CS_CHMFILE_GET_PRIVATE (self)->fixed_font;
}

void
cs_chmfile_set_fixed_font(CsChmfile *self, const gchar *font_name)
{
        CsChmfilePrivate *priv = CS_CHMFILE_GET_PRIVATE (self);

        g_free(priv->fixed_font);

        priv->fixed_font = g_strdup(font_name);
}

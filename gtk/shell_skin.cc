///////////////////////////////////////////////////////////////////////////////
// Free42 -- an HP-42S calculator simulator
// Copyright (C) 2004-2018  Thomas Okken
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2,
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see http://www.gnu.org/licenses/.
///////////////////////////////////////////////////////////////////////////////

#include <gtk/gtk.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include "shell_skin.h"
#include "shell_main.h"
#include "shell_loadimage.h"
#include "core_main.h"


/**************************/
/* Skin description stuff */
/**************************/

typedef struct {
    int x, y;
} SkinPoint;

typedef struct {
    int x, y, width, height;
} SkinRect;

typedef struct {
    int code, shifted_code;
    SkinRect sens_rect;
    SkinRect disp_rect;
    SkinPoint src;
} SkinKey;

#define SKIN_MAX_MACRO_LENGTH 63

typedef struct _SkinMacro {
    int code;
    unsigned char macro[SKIN_MAX_MACRO_LENGTH + 1];
    struct _SkinMacro *next;
} SkinMacro;

typedef struct {
    SkinRect disp_rect;
    SkinPoint src;
} SkinAnnunciator;

static SkinRect skin;
static SkinPoint display_loc;
static SkinPoint display_scale;
static SkinColor display_bg, display_fg;
static SkinKey *keylist = NULL;
static int nkeys = 0;
static int keys_cap = 0;
static SkinMacro *macrolist = NULL;
static SkinAnnunciator annunciators[7];

static FILE *external_file;
static long builtin_length;
static long builtin_pos;
static const unsigned char *builtin_file;

static GdkPixbuf *skin_image = NULL;
static int skin_y;
static int skin_type;
static const SkinColor *skin_cmap;

static GdkPixbuf *disp_image = NULL;

static char skin_label_buf[1024];
static int skin_label_pos;

static keymap_entry *keymap = NULL;
static int keymap_length;

static bool display_enabled = true;


/**********************************************************/
/* Linked-in skins; defined in the skins.c, which in turn */
/* is generated by skin2c.c under control of skin2c.conf  */
/**********************************************************/

extern const int skin_count;
extern const char *skin_name[];
extern const long skin_layout_size[];
extern const unsigned char *skin_layout_data[];
extern const long skin_bitmap_size[];
extern const unsigned char *skin_bitmap_data[];


/*******************/
/* Local functions */
/*******************/

static void addMenuItem(GtkMenu *menu, const char *name);
static void selectSkinCB(GtkWidget *w, gpointer cd);
static int skin_open(const char *name, int open_layout);
static int skin_gets(char *buf, int buflen);
static void skin_close();


static void addMenuItem(GtkMenu *menu, const char *name) {
    bool checked = false;
    if (state.skinName[0] == 0) {
        strcpy(state.skinName, name);
        checked = true;
    } else if (strcmp(state.skinName, name) == 0)
        checked = true;

    GtkWidget *w = gtk_check_menu_item_new_with_label(name);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w), checked);

    // Apparently, there is no way to retrieve the label from a menu item,
    // so I have to store them and pass them to the callback explicitly.
    char *lbl = skin_label_buf + skin_label_pos;
    strcpy(lbl, name);
    skin_label_pos += strlen(name) + 1;
    g_signal_connect(G_OBJECT(w), "activate",
                     G_CALLBACK(selectSkinCB), (gpointer) lbl);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), w);
    gtk_widget_show(w);
}

static void selectSkinCB(GtkWidget *w, gpointer cd) {
    char *name = (char *) cd;
    if (strcmp(state.skinName, name) != 0) {
        int w, h;
        strcpy(state.skinName, name);
        skin_load(&w, &h);
        core_repaint_display();
        gtk_widget_set_size_request(calc_widget, w, h);
        gtk_widget_queue_draw(calc_widget);
    }
}

static int skin_open(const char *name, int open_layout) {
    int i;
    char namebuf[1024];

    /* Look for built-in skin first */
    for (i = 0; i < skin_count; i++) {
        if (strcmp(name, skin_name[i]) == 0) {
            external_file = NULL;
            builtin_pos = 0;
            if (open_layout) {
                builtin_length = skin_layout_size[i];
                builtin_file = skin_layout_data[i];
            } else {
                builtin_length = skin_bitmap_size[i];
                builtin_file = skin_bitmap_data[i];
            }
            return 1;
        }
    }

    /* name did not match a built-in skin; look for file */
    snprintf(namebuf, 1024, "%s/%s.%s", free42dirname, name,
                                        open_layout ? "layout" : "gif");
    external_file = fopen(namebuf, "r");
    return external_file != NULL;
}

int skin_getchar() {
    if (external_file != NULL)
        return fgetc(external_file);
    else if (builtin_pos < builtin_length)
        return builtin_file[builtin_pos++];
    else
        return EOF;
}

static int skin_gets(char *buf, int buflen) {
    int p = 0;
    int eof = -1;
    int comment = 0;
    while (p < buflen - 1) {
        int c = skin_getchar();
        if (eof == -1)
            eof = c == EOF;
        if (c == EOF || c == '\n' || c == '\r')
            break;
        /* Remove comments */
        if (c == '#')
            comment = 1;
        if (comment)
            continue;
        /* Suppress leading spaces */
        if (p == 0 && isspace(c))
            continue;
        buf[p++] = c;
    }
    buf[p++] = 0;
    return p > 1 || !eof;
}

static void skin_close() {
    if (external_file != NULL)
        fclose(external_file);
}

static int case_insens_comparator(const void *a, const void *b) {
    return strcasecmp(*(const char **) a, *(const char **) b);
}

void skin_menu_update(GtkWidget *w) {
    int i, j;

    GtkMenu *skin_menu = (GtkMenu *) gtk_menu_item_get_submenu(GTK_MENU_ITEM(w));
    GList *children = gtk_container_get_children(GTK_CONTAINER(skin_menu));
    GList *item = children;
    while (item != NULL) {
        gtk_widget_destroy(GTK_WIDGET(item->data));
        item = item->next;
    }
    g_list_free(children);

    skin_label_pos = 0;

    for (i = 0; i < skin_count; i++)
        addMenuItem(skin_menu, skin_name[i]);

    DIR *dir = opendir(free42dirname);
    if (dir == NULL)
        return;

    struct dirent *dent;
    char *skinname[100];
    int nskins = 0;
    while ((dent = readdir(dir)) != NULL && nskins < 100) {
        int namelen = strlen(dent->d_name);
        char *skn;
        if (namelen < 7)
            continue;
        if (strcmp(dent->d_name + namelen - 7, ".layout") != 0)
            continue;
        skn = (char *) malloc(namelen - 6);
        // TODO - handle memory allocation failure
        memcpy(skn, dent->d_name, namelen - 7);
        skn[namelen - 7] = 0;
        skinname[nskins++] = skn;
    }
    closedir(dir);

    qsort(skinname, nskins, sizeof(char *), case_insens_comparator);
    bool have_separator = false;
    for (i = 0; i < nskins; i++) {
        for (j = 0; j < skin_count; j++)
            if (strcmp(skinname[i], skin_name[j]) == 0)
                goto skip;
        if (!have_separator) {
            GtkWidget *w = gtk_separator_menu_item_new();
            gtk_menu_shell_append(GTK_MENU_SHELL(skin_menu), w);
            gtk_widget_show(w);
            have_separator = true;
        }
        addMenuItem(skin_menu, skinname[i]);
        skip:
        free(skinname[i]);
    }
}

void skin_load(int *width, int *height) {
    char line[1024];
    int success;
    int lineno = 0;

    if (state.skinName[0] == 0) {
        fallback_on_1st_builtin_skin:
        strcpy(state.skinName, skin_name[0]);
    }

    /*************************/
    /* Load skin description */
    /*************************/

    if (!skin_open(state.skinName, 1))
        goto fallback_on_1st_builtin_skin;

    if (keylist != NULL)
        free(keylist);
    keylist = NULL;
    nkeys = 0;
    keys_cap = 0;

    while (macrolist != NULL) {
        SkinMacro *m = macrolist->next;
        free(macrolist);
        macrolist = m;
    }

    if (keymap != NULL)
        free(keymap);
    keymap = NULL;
    keymap_length = 0;
    int kmcap = 0;

    while (skin_gets(line, 1024)) {
        lineno++;
        if (*line == 0)
            continue;
        if (strncasecmp(line, "skin:", 5) == 0) {
            int x, y, width, height;
            if (sscanf(line + 5, " %d,%d,%d,%d", &x, &y, &width, &height) == 4){
                skin.x = x;
                skin.y = y;
                skin.width = width;
                skin.height = height;
            }
        } else if (strncasecmp(line, "display:", 8) == 0) {
            int x, y, xscale, yscale;
            unsigned long bg, fg;
            if (sscanf(line + 8, " %d,%d %d %d %lx %lx", &x, &y,
                                            &xscale, &yscale, &bg, &fg) == 6) {
                display_loc.x = x;
                display_loc.y = y;
                display_scale.x = xscale;
                display_scale.y = yscale;
                display_bg.r = (unsigned char) (bg >> 16);
                display_bg.g = (unsigned char) (bg >> 8);
                display_bg.b = (unsigned char) bg;
                display_fg.r = (unsigned char) (fg >> 16);
                display_fg.g = (unsigned char) (fg >> 8);
                display_fg.b = (unsigned char) fg;
            }
        } else if (strncasecmp(line, "key:", 4) == 0) {
            char keynumbuf[20];
            int keynum, shifted_keynum;
            int sens_x, sens_y, sens_width, sens_height;
            int disp_x, disp_y, disp_width, disp_height;
            int act_x, act_y;
            if (sscanf(line + 4, " %s %d,%d,%d,%d %d,%d,%d,%d %d,%d",
                                 keynumbuf,
                                 &sens_x, &sens_y, &sens_width, &sens_height,
                                 &disp_x, &disp_y, &disp_width, &disp_height,
                                 &act_x, &act_y) == 11) {
                int n = sscanf(keynumbuf, "%d,%d", &keynum, &shifted_keynum);
                if (n > 0) {
                    if (n == 1)
                        shifted_keynum = keynum;
                    SkinKey *key;
                    if (nkeys == keys_cap) {
                        keys_cap += 50;
                        keylist = (SkinKey *)
                                realloc(keylist, keys_cap * sizeof(SkinKey));
                        // TODO - handle memory allocation failure
                    }
                    key = keylist + nkeys;
                    key->code = keynum;
                    key->shifted_code = shifted_keynum;
                    key->sens_rect.x = sens_x;
                    key->sens_rect.y = sens_y;
                    key->sens_rect.width = sens_width;
                    key->sens_rect.height = sens_height;
                    key->disp_rect.x = disp_x;
                    key->disp_rect.y = disp_y;
                    key->disp_rect.width = disp_width;
                    key->disp_rect.height = disp_height;
                    key->src.x = act_x;
                    key->src.y = act_y;
                    nkeys++;
                }
            }
        } else if (strncasecmp(line, "macro:", 6) == 0) {
            char *tok = strtok(line + 6, " \t");
            int len = 0;
            SkinMacro *macro = NULL;
            while (tok != NULL) {
                char *endptr;
                long n = strtol(tok, &endptr, 10);
                if (*endptr != 0) {
                    /* Not a proper number; ignore this macro */
                    if (macro != NULL) {
                        free(macro);
                        macro = NULL;
                        break;
                    }
                }
                if (macro == NULL) {
                    if (n < 38 || n > 255)
                        /* Macro code out of range; ignore this macro */
                        break;
                    macro = (SkinMacro *) malloc(sizeof(SkinMacro));
                    // TODO - handle memory allocation failure
                    macro->code = n;
                } else if (len < SKIN_MAX_MACRO_LENGTH) {
                    if (n < 1 || n > 37) {
                        /* Key code out of range; ignore this macro */
                        free(macro);
                        macro = NULL;
                        break;
                    }
                    macro->macro[len++] = n;
                }
                tok = strtok(NULL, " \t");
            }
            if (macro != NULL) {
                macro->macro[len++] = 0;
                macro->next = macrolist;
                macrolist = macro;
            }
        } else if (strncasecmp(line, "annunciator:", 12) == 0) {
            int annnum;
            int disp_x, disp_y, disp_width, disp_height;
            int act_x, act_y;
            if (sscanf(line + 12, " %d %d,%d,%d,%d %d,%d",
                                  &annnum,
                                  &disp_x, &disp_y, &disp_width, &disp_height,
                                  &act_x, &act_y) == 7) {
                if (annnum >= 1 && annnum <= 7) {
                    SkinAnnunciator *ann = annunciators + (annnum - 1);
                    ann->disp_rect.x = disp_x;
                    ann->disp_rect.y = disp_y;
                    ann->disp_rect.width = disp_width;
                    ann->disp_rect.height = disp_height;
                    ann->src.x = act_x;
                    ann->src.y = act_y;
                }
            }
        } else if (strchr(line, ':') != NULL) {
            keymap_entry *entry = parse_keymap_entry(line, lineno);
            if (entry != NULL) {
                if (keymap_length == kmcap) {
                    kmcap += 50;
                    keymap = (keymap_entry *)
                                realloc(keymap, kmcap * sizeof(keymap_entry));
                    // TODO - handle memory allocation failure
                }
                memcpy(keymap + (keymap_length++), entry, sizeof(keymap_entry));
            }
        }
    }

    skin_close();

    /********************/
    /* Load skin bitmap */
    /********************/

    if (!skin_open(state.skinName, 0))
        goto fallback_on_1st_builtin_skin;

    /* shell_loadimage() calls skin_getchar() to load the image from the
     * compiled-in or on-disk file; it calls skin_init_image(),
     * skin_put_pixels(), and skin_finish_image() to create the in-memory
     * representation.
     */
    success = shell_loadimage();
    skin_close();

    if (!success)
        goto fallback_on_1st_builtin_skin;

    *width = skin.width;
    *height = skin.height;

    /********************************/
    /* (Re)build the display bitmap */
    /********************************/

    if (disp_image != NULL)
        g_object_unref(disp_image);
    disp_image = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                131 * display_scale.x,
                                16 * display_scale.y);
    guint32 p = (display_bg.r << 24)
                    | (display_bg.g << 16) | (display_bg.b << 8);
    gdk_pixbuf_fill(disp_image, p);
}

int skin_init_image(int type, int ncolors, const SkinColor *colors,
                    int width, int height) {
    if (skin_image != NULL) {
        g_object_unref(skin_image);
        skin_image = NULL;
    }

    skin_image = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);

    skin_y = 0;
    skin_type = type;
    skin_cmap = colors;
    return 1;
}

void skin_put_pixels(unsigned const char *data) {
    guchar *pix = gdk_pixbuf_get_pixels(skin_image);
    int bytesperline = gdk_pixbuf_get_rowstride(skin_image);
    int width = gdk_pixbuf_get_width(skin_image);
    guchar *p = pix + skin_y * bytesperline;

    if (skin_type == IMGTYPE_MONO) {
        for (int x = 0; x < width; x++) {
            guchar c;
            if ((data[x >> 3] & (1 << (x & 7))) == 0)
                c = 0;
            else
                c = 255;
            *p++ = c;
            *p++ = c;
            *p++ = c;
        }
    } else if (skin_type == IMGTYPE_GRAY) {
        for (int x = 0; x < width; x++) {
            guchar c = data[x];
            *p++ = c;
            *p++ = c;
            *p++ = c;
        }
    } else if (skin_type == IMGTYPE_COLORMAPPED) {
        for (int x = 0; x < width; x++) {
            guchar c = data[x];
            *p++ = skin_cmap[c].r;
            *p++ = skin_cmap[c].g;
            *p++ = skin_cmap[c].b;
        }
    } else { // skin_type == IMGTYPE_TRUECOLOR
        int xx = 0;
        for (int x = 0; x < width; x++) {
            xx++;
            *p++ = data[xx++];
            *p++ = data[xx++];
            *p++ = data[xx++];
        }
    }

    skin_y++;
}

void skin_finish_image() {
    // Nothing to do.
}

void skin_repaint() {
    draw_pixbuf(gtk_widget_get_window(calc_widget), skin_image, skin.x, skin.y, 0, 0, skin.width, skin.height);
}

void skin_repaint_annunciator(int which, bool state) {
    if (!display_enabled)
        return;
    SkinAnnunciator *ann = annunciators + (which - 1);
    if (state)
        draw_pixbuf(gtk_widget_get_window(calc_widget), skin_image,
                        ann->src.x, ann->src.y,
                        ann->disp_rect.x, ann->disp_rect.y,
                    ann->disp_rect.width, ann->disp_rect.height);
    else
        draw_pixbuf(gtk_widget_get_window(calc_widget), skin_image,
                        ann->disp_rect.x, ann->disp_rect.y,
                        ann->disp_rect.x, ann->disp_rect.y,
                    ann->disp_rect.width, ann->disp_rect.height);
}

void skin_find_key(int x, int y, bool cshift, int *skey, int *ckey) {
    int i;
    if (core_menu()
            && x >= display_loc.x
            && x < display_loc.x + 131 * display_scale.x
            && y >= display_loc.y + 9 * display_scale.y
            && y < display_loc.y + 16 * display_scale.y) {
        int softkey = (x - display_loc.x) / (22 * display_scale.x) + 1;
        *skey = -1 - softkey;
        *ckey = softkey;
        return;
    }
    for (i = 0; i < nkeys; i++) {
        SkinKey *k = keylist + i;
        int rx = x - k->sens_rect.x;
        int ry = y - k->sens_rect.y;
        if (rx >= 0 && rx < k->sens_rect.width
                && ry >= 0 && ry < k->sens_rect.height) {
            *skey = i;
            *ckey = cshift ? k->shifted_code : k->code;
            return;
        }
    }
    *skey = -1;
    *ckey = 0;
}

int skin_find_skey(int ckey) {
    int i;
    for (i = 0; i < nkeys; i++)
        if (keylist[i].code == ckey || keylist[i].shifted_code == ckey)
            return i;
    return -1;
}

unsigned char *skin_find_macro(int ckey) {
    SkinMacro *m = macrolist;
    while (m != NULL) {
        if (m->code == ckey)
            return m->macro;
        m = m->next;
    }
    return NULL;
}

unsigned char *skin_keymap_lookup(guint keyval, bool printable,
                                  bool ctrl, bool alt, bool shift, bool cshift,
                                  bool *exact) {
    int i;
    unsigned char *macro = NULL;
    for (i = 0; i < keymap_length; i++) {
        keymap_entry *entry = keymap + i;
        if (ctrl == entry->ctrl
                && alt == entry->alt
                && (printable || shift == entry->shift)
                && keyval == entry->keyval) {
            macro = entry->macro;
            if (cshift == entry->cshift) {
                *exact = true;
                return macro;
            }
        }
    }
    *exact = false;
    return macro;
}

void skin_repaint_key(int key, bool state) {
    SkinKey *k;

    // TODO: Test soft keys
    if (key >= -7 && key <= -2) {
        /* Soft key */
        if (!display_enabled)
            // Should never happen -- the display is only disabled during macro
            // execution, and softkey events should be impossible to generate
            // in that state. But, just staying on the safe side.
            return;
        key = -1 - key;
        int x = (key - 1) * 22 * display_scale.x;
        int y = 9 * display_scale.y;
        int width = 21 * display_scale.x;
        int height = 7 * display_scale.y;
        if (state) {
            // Construct a temporary pixbuf, create the inverted version of
            // the affected screen rectangle there, and blit it
            GdkPixbuf *tmpbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                               width, height);
            int s_bpl = gdk_pixbuf_get_rowstride(disp_image);
            int d_bpl = gdk_pixbuf_get_rowstride(tmpbuf);
            guchar *s1 = gdk_pixbuf_get_pixels(disp_image) + x * 3 + s_bpl * y;
            guchar *d1 = gdk_pixbuf_get_pixels(tmpbuf);
            for (int v = 0; v < height; v++) {
                guchar *src = s1;
                guchar *dst = d1;
                for (int h = 0; h < width; h++) {
                    unsigned char r = *src++;
                    unsigned char g = *src++;
                    unsigned char b = *src++;
                    if (r == display_bg.r && g == display_bg.g && b == display_bg.b) {
                        *dst++ = display_fg.r;
                        *dst++ = display_fg.g;
                        *dst++ = display_fg.b;
                    } else {
                        *dst++ = display_bg.r;
                        *dst++ = display_bg.g;
                        *dst++ = display_bg.b;
                    }
                }
                s1 += s_bpl;
                d1 += d_bpl;
            }
            draw_pixbuf(gtk_widget_get_window(calc_widget), tmpbuf,
                            0, 0,
                            display_loc.x + x, display_loc.y + y,
                        width, height);
        } else {
            // Repaint the screen
            draw_pixbuf(gtk_widget_get_window(calc_widget), disp_image,
                            x, y,
                            display_loc.x + x, display_loc.y + y,
                        width, height);
        }
        return;
    }

    if (key < 0 || key >= nkeys)
        return;
    k = keylist + key;
    if (state)
        draw_pixbuf(gtk_widget_get_window(calc_widget), skin_image,
                        k->src.x, k->src.y,
                        k->disp_rect.x, k->disp_rect.y,
                    k->disp_rect.width, k->disp_rect.height);
    else
        draw_pixbuf(gtk_widget_get_window(calc_widget), skin_image,
                        k->disp_rect.x, k->disp_rect.y,
                        k->disp_rect.x, k->disp_rect.y,
                    k->disp_rect.width, k->disp_rect.height);
}

void skin_display_blitter(const char *bits, int bytesperline, int x, int y,
                                     int width, int height) {
    guchar *pix = gdk_pixbuf_get_pixels(disp_image);
    int disp_bpl = gdk_pixbuf_get_rowstride(disp_image);
    int sx = display_scale.x;
    int sy = display_scale.y;

    for (int v = y; v < y + height; v++)
        for (int h = x; h < x + width; h++) {
            SkinColor c;
            if ((bits[v * bytesperline + (h >> 3)] & (1 << (h & 7))) != 0)
                c = display_fg;
            else
                c = display_bg;
            for (int vv = v * sy; vv < (v + 1) * sy; vv++) {
                guchar *p = pix + disp_bpl * vv;
                for (int hh = h * sx; hh < (h + 1) * sx; hh++) {
                    guchar *p2 = p + hh * 3;
                    p2[0] = c.r;
                    p2[1] = c.g;
                    p2[2] = c.b;
                }
            }
        }
    if (allow_paint && display_enabled)
        draw_pixbuf(gtk_widget_get_window(calc_widget), disp_image,
                        x * sx, y * sy,
                        display_loc.x + x * sx, display_loc.y + y * sy,
                    width * sx, height * sy);
}

void skin_repaint_display() {
    if (display_enabled)
        draw_pixbuf(gtk_widget_get_window(calc_widget), disp_image,
                        0, 0, display_loc.x, display_loc.y,
                    131 * display_scale.x, 16 * display_scale.y);
}

void skin_display_set_enabled(bool enable) {
    display_enabled = enable;
}

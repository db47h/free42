/*****************************************************************************
 * Free42 -- an HP-42S calculator simulator
 * Copyright (C) 2004-2025  Thomas Okken
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see http://www.gnu.org/licenses/.
 *****************************************************************************/

#import <UIKit/UIKit.h>

@class CalcView;

void skin_load(long *width, long *height);

void skin_repaint(CGRect *rect, bool shortcuts);
void skin_update_annunciator(int which, int state, CalcView *view);
bool skin_in_menu_area(int x, int y, bool *keyboard);
void skin_position_menu(UIView *view, UIAlertController *ctrl);
void skin_find_key(int x, int y, bool cshift, int *skey, int *ckey);
int skin_find_skey(int ckey, bool cshift);
unsigned char *skin_find_macro(int ckey, int *type);
unsigned char *skin_keymap_lookup(unsigned short keychar, bool printable,
                  bool ctrl, bool alt, bool numpad, bool shift, bool cshift,
                  bool *exact);
void skin_set_pressed_key(int skey, CalcView *view);
void skin_display_blitter(const char *bits, int bytesperline, int x, int y, int width, int height, CalcView *view);
void skin_repaint_display(CalcView *view);

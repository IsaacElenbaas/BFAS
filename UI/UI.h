#ifndef UI_H
#define UI_H
#include "types.h"

bool set_pixel(const point& p, int r, int g, int b, int a);
void paint_handle(const point& p);
void paint_line(const point& p1, const point& p2);
void paint_flush();
void repaint(bool now);
#endif

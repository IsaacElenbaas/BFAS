#ifndef PFAS_H
#define PFAS_H
extern int w, h;
#include "state.h"
#include "types.h"

extern point mouse;
void paint();
void detect_shapes();
void zoom(double amount);
void key_release(int key);
void mouse_move(int x, int y);
void mouse_press(bool left, int x, int y);
void mouse_release(bool left, int x, int y);
void mouse_double_click(bool left, int x, int y);
#endif

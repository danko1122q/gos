#ifndef DRV_MOUSE_H
#define DRV_MOUSE_H

void mouse_set_wheel(int on);
void mouse_set_bounds(int w, int h);
void mouse_byte(unsigned char b);
int  mouse_x(void);
int  mouse_y(void);

#endif

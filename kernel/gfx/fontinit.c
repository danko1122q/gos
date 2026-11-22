#include "gfx/font.h"

extern const UiFont font_data_chicago, font_data_geneva;

void font_init(void) {
    font_set_ui(FACE_CHICAGO, &font_data_chicago);
    font_set_ui(FACE_GENEVA, &font_data_geneva);
}

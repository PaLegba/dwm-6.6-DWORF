
// vertical_text_perfect_final.c - test vertical text
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main() {
    Display *dpy = XOpenDisplay(NULL);
    int scr = DefaultScreen(dpy);

    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr),
                                     50, 50, 1000, 1400, 0,
                                     BlackPixel(dpy, scr), WhitePixel(dpy, scr));

    XSelectInput(dpy, win, ExposureMask | KeyPressMask);
    XMapWindow(dpy, win);

    XftDraw *draw = XftDrawCreate(dpy, win, DefaultVisual(dpy, scr), DefaultColormap(dpy, scr));

    XftFont *font = XftFontOpenName(dpy, scr, "DejaVu Sans-64");
    if (!font) font = XftFontOpenName(dpy, scr, "sans-64");

    XftColor color;
    XRenderColor xrc = {0, 0, 0, 0xffff};
    XftColorAllocValue(dpy, DefaultVisual(dpy, scr), DefaultColormap(dpy, scr), &xrc, &color);

    const char *text = "Вертикальный текст\nповёрнутый на 90°\nпротив часовой стрелки\nКириллица работает\nКернинг тоже ок\nИдеально в 2025!";

    FT_Matrix matrix = { .xx = 0<<16, .xy = -1<<16, .yx = 1<<16, .yy = 0<<16 };

    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);
        if (ev.type == Expose) {
            XClearWindow(dpy, win);

            FT_Face face = XftLockFace(font);
            FT_Set_Transform(face, &matrix, NULL);

            int len = strlen(text);
            XftGlyphFontSpec *specs = calloc(len + 1, sizeof(XftGlyphFontSpec));

            int x = 900;   // нижняя точка начала текста
            int y = 200;   // левый отступ (после поворота — расстояние от левого края)

            int column_step = font->ascent + font->descent + 80;  // расстояние между колонками

            FT_UInt previous = 0;
            int pos = 0;

            for (const char *p = text; *p; ) {
                if (*p == '\n') {
                    x -= column_step;   // новая колонка левее
                    y = 200;
                    previous = 0;
                    p++;
                    continue;
                }

                // UTF-8 → UCS4
                FcChar32 c = 0;
                if ((*p & 0x80) == 0) c = *p++;
                else if ((*p & 0xE0) == 0xC0) { c = (*p++ & 0x1F) << 6; c |= (*p++ & 0x3F); }
                else if ((*p & 0xF0) == 0xE0) { c = (*p++ & 0x0F) << 12; c |= (*p++ & 0x3F) << 6; c |= (*p++ & 0x3F); }
                else { p++; continue; }

                FT_UInt glyph = XftCharIndex(dpy, font, c);
                if (!glyph) continue;

                // Кернинги (работают!)
                if (previous) {
                    FT_Vector kern;
                    FT_Get_Kerning(face, previous, glyph, FT_KERNING_DEFAULT, &kern);
                    y += kern.y >> 6;  // после поворота кернинг по Y!
                }

                specs[pos].font = font;
                specs[pos].glyph = glyph;
                specs[pos].x = x;
                specs[pos].y = y;
                pos++;

                XGlyphInfo gi;
                XftGlyphExtents(dpy, font, &glyph, 1, &gi);
                y += gi.yOff;   // ← ВОТ ОНА, ЗОЛОТАЯ СТРОКА!

                previous = glyph;
            }

            XftDrawGlyphFontSpec(draw, &color, specs, pos);
            free(specs);
            XftUnlockFace(font);
        }
//        if (ev.type == KeyPress) break;
    }

    XftFontClose(dpy, font);
    XftDrawDestroy(draw);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}

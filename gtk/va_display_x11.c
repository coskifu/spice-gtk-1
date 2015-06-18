/*
 * Copyright Flexible Software Solutions S.L. 2014
 */

#include <stdlib.h>
#include <string.h>
#include "tinyjpeg.h"
#include "va_display_x11.h"
#include <va/va_x11.h>
#include <cairo/cairo-xlib.h>
#include "spice-util.h"

struct display_private {
    Display *x11_dpy;
    Window root;
    Visual *visual;
    int depth;
    Pixmap pixmap;
    VARectangle dst_rect;
};

static VAStatus va_x11_open_display(tinyjpeg_session *session)
{
    Display *x11_dpy = XOpenDisplay(NULL);
    VAStatus va_status = VA_STATUS_ERROR_UNKNOWN;
    if (x11_dpy) {
        session->va_dpy = vaGetDisplay(x11_dpy);
        int major, minor;
        va_status = vaInitialize(session->va_dpy, &major, &minor);
        if (va_status == VA_STATUS_SUCCESS) {
            display_private *d = session->dpy_priv = malloc(sizeof(struct display_private));
            memset(d, 0, sizeof(struct display_private));
            d->x11_dpy = x11_dpy;
            int screen = XDefaultScreen(d->x11_dpy);
            d->depth = XDefaultDepth(d->x11_dpy, screen);
            d->root = XRootWindow(d->x11_dpy, screen); // TODO: Is this correct?
            d->visual = XDefaultVisual(d->x11_dpy, screen);
            d->pixmap = XCreatePixmap(d->x11_dpy, d->root, 1, 1, d->depth);
            d->dst_rect.width = d->dst_rect.height = 1;
        } else XCloseDisplay(x11_dpy);
    }
    return va_status;
}

static void va_x11_close_display(tinyjpeg_session *session)
{
    display_private *d = session->dpy_priv;
    if (d) {
        XFreePixmap(d->x11_dpy, d->pixmap);
        XCloseDisplay(d->x11_dpy);
        free(d);
    }
}

static VAStatus va_x11_put_surface(tinyjpeg_session *session, VASurfaceID surface)
{
    if (!session->dpy_priv)
        return VA_STATUS_ERROR_INVALID_DISPLAY;
    if (!session->va_dpy)
        return VA_STATUS_ERROR_INVALID_DISPLAY;
    if (surface == VA_INVALID_SURFACE)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    display_private *d = session->dpy_priv;
    VARectangle *src_rect = &session->src_rect;
    VARectangle *dst_rect = &session->dst_rect;
    VARectangle *cur_rect = &d->dst_rect;

    if (memcmp(cur_rect, dst_rect, sizeof(VARectangle)) != 0) {
        XFreePixmap(d->x11_dpy, d->pixmap);
        SPICE_DEBUG("Create pixmap for va surface");
        d->pixmap = XCreatePixmap(d->x11_dpy, d->root,
                                  dst_rect->width, dst_rect->height, d->depth);
        memcpy(cur_rect, dst_rect, sizeof(VARectangle));
    }

    return vaPutSurface(session->va_dpy, surface, d->pixmap,
                        0, 0, src_rect->width, src_rect->height,
                        0, 0, dst_rect->width, dst_rect->height,
                        NULL, 0, VA_FRAME_PICTURE);
}

void va_x11_draw_frame(tinyjpeg_session *session, cairo_t *cr)
{
    display_private *d = session->dpy_priv;
    cairo_surface_t *surface =
            cairo_xlib_surface_create(d->x11_dpy, d->pixmap, d->visual,
                                      d->dst_rect.width, d->dst_rect.height);

    cairo_save(cr);
    cairo_translate(cr, d->dst_rect.x, d->dst_rect.y);
    cairo_rectangle(cr, 0, 0, d->dst_rect.width, d->dst_rect.height);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_fill(cr);
    cairo_restore(cr);
    cairo_surface_destroy(surface);
}

static VADisplayHooks va_x11_display_hooks = {
    .open_display = va_x11_open_display,
    .close_display = va_x11_close_display,
    .put_surface = va_x11_put_surface,
};

void va_x11_set_display_hooks(void)
{
    set_va_display_hooks(&va_x11_display_hooks);
}

/*
 * Minimal WRaster subset for wmglobe.
 *
 * wmglobe 0.5 shipped with its own copies of the WRaster context and
 * conversion code, but still linked the full libwraster for image loading
 * and XImage helpers.  This file supplies only the handful of symbols it
 * actually calls.  Image files are decoded with stb_image (public domain)
 * instead of dragging in libwraster + libtiff + libpng + libjpeg + libungif.
 */

#include "wmglobe.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../../../stb/stb_image.h"

int RErrorCode = RERR_NONE;

RImage *
RCreateImage(unsigned width, unsigned height, int alpha)
{
	RImage *image;
	unsigned long n = (unsigned long) width * height;
	int i;

	image = calloc(1, sizeof(RImage));
	if (!image) {
		RErrorCode = RERR_NOMEMORY;
		return NULL;
	}
	image->width = width;
	image->height = height;
	for (i = 0; i < 4; i++) {
		if (i == 3 && !alpha)
			continue;
		image->data[i] = calloc(n ? n : 1, 1);
		if (!image->data[i]) {
			while (i-- > 0)
				free(image->data[i]);
			free(image);
			RErrorCode = RERR_NOMEMORY;
			return NULL;
		}
	}
	return image;
}

void
RClearImage(RImage *image, RColor *color)
{
	unsigned long i, n = (unsigned long) image->width * image->height;

	for (i = 0; i < n; i++) {
		image->data[0][i] = color->red;
		image->data[1][i] = color->green;
		image->data[2][i] = color->blue;
		if (image->data[3])
			image->data[3][i] = color->alpha;
	}
}

void
RPutPixel(RImage *image, int x, int y, RColor *color)
{
	unsigned long ofs;

	if (x < 0 || y < 0 || x >= (int) image->width || y >= (int) image->height)
		return;
	ofs = (unsigned long) y * image->width + x;
	image->data[0][ofs] = color->red;
	image->data[1][ofs] = color->green;
	image->data[2][ofs] = color->blue;
	if (image->data[3])
		image->data[3][ofs] = color->alpha;
}

static unsigned int
mask_shift(unsigned long mask)
{
	unsigned int shift = 0;

	if (!mask)
		return 0;
	while (!(mask & 1)) {
		mask >>= 1;
		shift++;
	}
	return shift;
}

static unsigned char
mask_scale(unsigned long pixel, unsigned long mask)
{
	unsigned long v, max;

	if (!mask)
		return 0;
	v = (pixel & mask) >> mask_shift(mask);
	max = mask >> mask_shift(mask);
	if (!max)
		return 0;
	return (unsigned char) ((v * 255 + max / 2) / max);
}

static void
image_pixel_to_rgb(RContext *ctx, XImage *xi, int x, int y,
		   unsigned char *r, unsigned char *g, unsigned char *b)
{
	unsigned long pixel = XGetPixel(xi, x, y);
	unsigned long rm, gm, bm;

	if (ctx->visual->class == TrueColor || ctx->visual->class == DirectColor) {
		rm = ctx->visual->red_mask;
		gm = ctx->visual->green_mask;
		bm = ctx->visual->blue_mask;
		if (!rm && xi->red_mask)
			rm = xi->red_mask;
		if (!gm && xi->green_mask)
			gm = xi->green_mask;
		if (!bm && xi->blue_mask)
			bm = xi->blue_mask;
		*r = mask_scale(pixel, rm);
		*g = mask_scale(pixel, gm);
		*b = mask_scale(pixel, bm);
	} else {
		XColor color;

		color.pixel = pixel;
		XQueryColor(ctx->dpy, ctx->cmap, &color);
		*r = color.red >> 8;
		*g = color.green >> 8;
		*b = color.blue >> 8;
	}
}

RImage *
RGetImageFromXPMData(RContext *ctx, char **data)
{
	XpmAttributes attributes;
	XImage *xi = NULL;
	XImage *mask = NULL;
	RImage *image;
	int x, y, status;

	memset(&attributes, 0, sizeof attributes);
	attributes.visual = ctx->visual;
	attributes.depth = ctx->depth;
	attributes.colormap = ctx->cmap;
	attributes.valuemask = XpmVisual | XpmDepth | XpmColormap;

	status = XpmCreateImageFromData(ctx->dpy, data, &xi, &mask, &attributes);
	if (status != XpmSuccess || !xi) {
		RErrorCode = RERR_BADIMAGEFILE;
		return NULL;
	}

	image = RCreateImage(xi->width, xi->height, 1);
	if (image) {
		for (y = 0; y < xi->height; y++) {
			for (x = 0; x < xi->width; x++) {
				unsigned char r, g, b;
				unsigned long ofs = (unsigned long) y * xi->width + x;

				image_pixel_to_rgb(ctx, xi, x, y, &r, &g, &b);
				image->data[0][ofs] = r;
				image->data[1][ofs] = g;
				image->data[2][ofs] = b;
				image->data[3][ofs] = 255;
			}
		}
	}
	if (mask)
		XDestroyImage(mask);
	XDestroyImage(xi);
	return image;
}

RImage *
RLoadImage(RContext *ctx, char *file, int index)
{
	RImage *image;
	unsigned char *pixels;
	int w, h, channels, x, y;

	(void) ctx;
	if (index != 0) {
		RErrorCode = RERR_BADINDEX;
		return NULL;
	}
	if (access(file, R_OK) != 0) {
		RErrorCode = RERR_OPEN;
		return NULL;
	}
	pixels = stbi_load(file, &w, &h, &channels, 4);
	if (!pixels) {
		RErrorCode = RERR_BADFORMAT;
		return NULL;
	}
	image = RCreateImage(w, h, 1);
	if (image) {
		for (y = 0; y < h; y++) {
			for (x = 0; x < w; x++) {
				unsigned long ofs = (unsigned long) y * w + x;
				unsigned char *src = pixels + ofs * 4;

				image->data[0][ofs] = src[0];
				image->data[1][ofs] = src[1];
				image->data[2][ofs] = src[2];
				image->data[3][ofs] = src[3];
			}
		}
	}
	stbi_image_free(pixels);
	return image;
}

RXImage *
RCreateXImage(RContext *ctx, int depth, unsigned width, unsigned height)
{
	RXImage *ximg;

	ximg = calloc(1, sizeof(RXImage));
	if (!ximg) {
		RErrorCode = RERR_NOMEMORY;
		return NULL;
	}
	ximg->image = XCreateImage(ctx->dpy, ctx->visual, depth, ZPixmap, 0,
				   NULL, width, height, 32, 0);
	if (!ximg->image) {
		free(ximg);
		RErrorCode = RERR_XERROR;
		return NULL;
	}
	ximg->image->data = calloc(ximg->image->bytes_per_line * height, 1);
	if (!ximg->image->data) {
		XDestroyImage(ximg->image);
		free(ximg);
		RErrorCode = RERR_NOMEMORY;
		return NULL;
	}
	return ximg;
}

void
RDestroyXImage(RContext *ctx, RXImage *ximage)
{
	(void) ctx;
	if (!ximage)
		return;
	if (ximage->image)
		XDestroyImage(ximage->image);
	free(ximage);
}

void
RPutXImage(RContext *ctx, Drawable d, GC gc, RXImage *ximage,
	   int src_x, int src_y, int dest_x, int dest_y,
	   unsigned width, unsigned height)
{
	XPutImage(ctx->dpy, d, gc, ximage->image,
		  src_x, src_y, dest_x, dest_y, width, height);
}

int
RConvertImage(RContext *ctx, RImage *image, Pixmap *pixmap)
{
	*pixmap = XCreatePixmap(ctx->dpy, ctx->drawable,
				image->width, image->height, ctx->depth);
	if (!*pixmap) {
		RErrorCode = RERR_XERROR;
		return False;
	}
	return myRConvertImage(ctx, image, pixmap);
}

const char *
RMessageForError(int errorCode)
{
	switch (errorCode) {
	case RERR_NONE:		return "no error";
	case RERR_OPEN:		return "can't open file";
	case RERR_READ:		return "error reading file";
	case RERR_WRITE:	return "error writing file";
	case RERR_NOMEMORY:	return "out of memory";
	case RERR_NOCOLOR:	return "out of color cells";
	case RERR_BADIMAGEFILE:	return "corrupted or invalid image file";
	case RERR_BADFORMAT:	return "unknown image file format";
	case RERR_BADINDEX:	return "no such image index in file";
	case RERR_XERROR:	return "X error";
	default:		return "internal error";
	}
}

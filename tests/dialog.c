/* parent + transient dialog for the modal regression test */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdio.h>

static Display *dpy;
static Window parent, dialog;
static Atom wmDelete, wmProtocols;

static void
setup(Window w, const char *title)
{
	XStoreName(dpy, w, title);
	XSetWMProtocols(dpy, w, &wmDelete, 1);
}

int
main(void)
{
	XEvent ev;
	Atom type, dialogType;
	XWMHints *hints;

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "no display\n");
		return 1;
	}
	wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmProtocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
	type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	dialogType = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);

	parent = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
		100, 100, 420, 320, 0, 0, 0xffffff);
	setup(parent, "PARENT");
	XMapWindow(dpy, parent);

	dialog = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
		220, 180, 240, 140, 0, 0, 0xeeeeee);
	setup(dialog, "DIALOG");
	XSetTransientForHint(dpy, dialog, parent);
	XChangeProperty(dpy, dialog, type, XA_ATOM, 32, PropModeReplace,
		(unsigned char *)&dialogType, 1);
	hints = XAllocWMHints();
	hints->flags = InputHint;
	hints->input = True;
	XSetWMHints(dpy, dialog, hints);
	XFree(hints);
	XMapWindow(dpy, dialog);
	XFlush(dpy);

	for (;;) {
		XNextEvent(dpy, &ev);
		if (ev.type == ClientMessage) {
			XClientMessageEvent *cme = &ev.xclient;
			if (cme->message_type == wmProtocols &&
			    (Atom)cme->data.l[0] == wmDelete) {
				if (cme->window == dialog) {
					XDestroyWindow(dpy, dialog);
					dialog = None;
					printf("dialog closed, parent alive\n");
					fflush(stdout);
				} else {
					break;
				}
			}
		} else if (ev.type == DestroyNotify &&
			   ev.xdestroywindow.window == parent) {
			break;
		}
	}
	XCloseDisplay(dpy);
	return 0;
}

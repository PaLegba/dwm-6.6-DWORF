/// thoth (l) loosely based on tilemovemouse.c for dwm 5.8
/// requires push.c to be included before this file due to using pushup/pushdown functions

#ifndef INRECT
#define INRECT(X,Y,RX,RY,RW,RH) ((X) >= (RX) && (X) < (RX) + (RW) && (Y) >= (RY) && (Y) < (RY) + (RH))
#endif /*INRECT*/

static Client *
tiledragtarget(Client *c, int x, int y)
{
        Client *d;

        for (d = nexttiled(c->mon->clients); d; d = nexttiled(d->next))
                if (d != c && INRECT(x, y, d->x, d->y, WIDTH(d), HEIGHT(d)))
                        return d;
        return NULL;
}

static void
tiledragreorder(Client *c, Client *target)
{
        Client *d, *prev = NULL;
        int before = 0;

        if (!c || !target || c == target)
                return;

        for (d = c->mon->clients; d; d = d->next) {
                if (d == target) {
                        before = 1;
                        break;
                }
                if (d == c)
                        break;
        }

        detach(c);
        for (d = c->mon->clients; d && d != target; d = d->next)
                prev = d;

        if (!d) {
                attach(c);
        } else if (before) {
                c->next = d;
                if (prev)
                        prev->next = c;
                else
                        c->mon->clients = c;
        } else {
                c->next = d->next;
                d->next = c;
        }

        focus(c);
        arrange(c->mon);
}

/// reorder only when the pointer enters a different tile
void
tilemovemouse(const Arg *arg) {
        Client *c, *target, *lasttarget = NULL;
        XEvent ev;
        int x, y;

        if(!(c = selmon->sel))
                return;

        if(c->isfloating || !c->mon->lt[c->mon->sellt]->arrange){
                movemouse(NULL);
                return;
        }

        /* Используем None вместо cursor[CurMove] — не трогаем глобальный cursor */
        if(XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
           None, None, CurrentTime) != GrabSuccess)
                return;

        do {
                XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
                switch (ev.type) {
                case ConfigureRequest:
                case Expose:
                case MapRequest:
                        handler[ev.type](&ev);
                        break;
                case MotionNotify:
                        x = ev.xmotion.x;
                        y = ev.xmotion.y;
                        target = tiledragtarget(c, x, y);
                        if (target != lasttarget) {
                                if (target)
                                        tiledragreorder(c, target);
                                /*
                                 * Recompute after the layout changed.  This
                                 * absorbs geometry changes caused by cfact
                                 * differences and prevents an immediate
                                 * second reorder from the same mouse event.
                                 */
                                lasttarget = tiledragtarget(c, x, y);
                        }
                        break;
                }
        } while(ev.type != ButtonRelease);

        XUngrabPointer(dpy, CurrentTime);
}

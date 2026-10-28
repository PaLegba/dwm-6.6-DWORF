static void
bstack(Monitor *m) {
	unsigned int i, j, n, nm, shm, promote;
	float mfacts = 0, sfacts = 0;
	int x, y, cw, h, mh, rem;
	Client *c, **cl;
	unsigned char *used;

	if (!(cl = tiledclients(m, &n)))
		return;

	nm = MIN(n, m->nmaster);
	if (n > m->nmaster)
		mh = m->nmaster ? m->mfact * m->wh : 0;
	else
		mh = m->wh;
	used = ecalloc(n, sizeof(unsigned char));
	shm = 0;

	for (i = 0; i < nm; i++) {
		mfacts += cl[i]->cfact;
		if (cl[i]->isshaded)
			shm++;
	}
	promote = 0;
	for (i = nm; i < n; i++)
		if (!cl[i]->isshaded)
			promote++;
	promote = MIN(promote, shm);

	/* master row */
	rem = m->ww;
	for (i = 0; i < nm; i++)
		if (cl[i]->isshaded)
			rem -= titlebar_width + 2 * cl[i]->bw + 2 * (int)gappx;
	x = m->wx;
	y = m->wy;
	for (i = 0; i < nm; i++) {
		c = cl[i];
		cw = mfacts > 0 ? (int)(rem * (c->cfact / mfacts)) : 0;
		if (c->isshaded) {
			c->shadevertical = 1;
			resizetile(c, x, y, titlebar_width + 2 * (int)gappx, mh - 2 * c->bw, 0);
			x += titlebar_width + 2 * c->bw + 2 * (int)gappx;
			if (promote > 0) {
				for (j = nm; j < n; j++)
					if (!used[j] && !cl[j]->isshaded)
						break;
				if (j < n) {
					Client *p = cl[j];
					used[j] = 1;
					resizetile(p, x, y, MAX(1, cw) - 2 * p->bw,
						mh - 2 * p->bw, 0);
					x += cw;
					promote--;
				}
			}
		} else {
			resizetile(c, x, y, MAX(1, cw) - 2 * c->bw,
				mh - 2 * c->bw, 0);
			x += cw;
		}
		mfacts -= c->cfact;
		rem -= cw;
	}

	/* stack row */
	rem = m->ww;
	for (i = nm; i < n; i++) {
		if (used[i])
			continue;
		if (cl[i]->isshaded)
			rem -= titlebar_width + 2 * cl[i]->bw + 2 * (int)gappx;
		else
			sfacts += cl[i]->cfact;
	}
	x = m->wx;
	y = m->wy + mh;
	h = m->wh - mh;
	for (i = nm; i < n; i++) {
		c = cl[i];
		if (used[i])
			continue;
		if (c->isshaded) {
			c->shadevertical = 1;
			resizetile(c, x, y, titlebar_width + 2 * (int)gappx, h - 2 * c->bw, 0);
			x += titlebar_width + 2 * c->bw + 2 * (int)gappx;
		} else {
			cw = sfacts > 0 ? (int)(rem * (c->cfact / sfacts)) : 0;
			resizetile(c, x, y, MAX(1, cw) - 2 * c->bw,
				h - 2 * c->bw, 0);
			x += cw;
			rem -= cw;
			sfacts -= c->cfact;
		}
	}

	free(used);
	free(cl);
}

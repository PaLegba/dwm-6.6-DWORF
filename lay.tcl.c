void
tcl(Monitor *m)
{
	unsigned int i, n, nright;
	float facts;
	int mw, sw, x, y, ch, rem;
	Client **cl;

	if (!(cl = tiledclients(m, &n)))
		return;

	mw = (int)(m->mfact * m->ww);
	mw = MAX(bh, MIN(mw, m->ww - 2 * bh));

	if (n == 1) {
		resize(cl[0], m->wx, m->wy, m->ww - 2 * cl[0]->bw,
			m->wh - 2 * cl[0]->bw, False);
		free(cl);
		return;
	}

	if (n == 2) {
		resize(cl[0], m->wx, m->wy, mw - 2 * cl[0]->bw,
			m->wh - 2 * cl[0]->bw, False);
		resize(cl[1], m->wx + mw, m->wy,
			m->ww - mw - 2 * cl[1]->bw,
			m->wh - 2 * cl[1]->bw, False);
		free(cl);
		return;
	}

	sw = (m->ww - mw) / 2;
	/* layout x is root-relative: always include m->wx */
	resize(cl[0], m->wx + sw, m->wy, mw - 2 * cl[0]->bw,
		m->wh - 2 * cl[0]->bw, False);

	nright = (n - 1) / 2;

	/* right column, then left column; heights follow cfact */
	x = m->wx + mw + sw;
	y = m->wy;
	rem = m->wh;
	for (i = 1, facts = 0; i < 1 + nright; i++)
		facts += cl[i]->cfact;
	for (i = 1; i < 1 + nright; i++) {
		ch = facts > 0 ? (int)(rem * (cl[i]->cfact / facts)) : 0;
		resize(cl[i], x, y, sw - 2 * cl[i]->bw,
			MAX(1, ch) - 2 * cl[i]->bw, False);
		y += MAX(1, ch);
		rem -= ch;
		facts -= cl[i]->cfact;
	}

	x = m->wx;
	y = m->wy;
	rem = m->wh;
	for (i = 1 + nright, facts = 0; i < n; i++)
		facts += cl[i]->cfact;
	for (i = 1 + nright; i < n; i++) {
		ch = facts > 0 ? (int)(rem * (cl[i]->cfact / facts)) : 0;
		resize(cl[i], x, y, sw - 2 * cl[i]->bw,
			MAX(1, ch) - 2 * cl[i]->bw, False);
		y += MAX(1, ch);
		rem -= ch;
		facts -= cl[i]->cfact;
	}

	free(cl);
}

void
grid(Monitor *m) {
	unsigned int i, n, rows, cols, r;
	float sfacts;
	int x, y, y1, cw, h, rem;
	Client *c, **cl;

	if (!(cl = tiledclients(m, &n)))
		return;

	/* grid dimensions */
	for(rows = 0; rows <= n/2; rows++)
		if(rows*rows >= n)
			break;
	cols = (rows && (rows - 1) * rows >= n) ? rows - 1 : rows;
	if(rows == 0)
		rows = 1;
	if(cols == 0)
		cols = 1;

	for(r = 0; r < rows; r++) {
		y = m->wy + (int)(m->wh * r / rows);
		y1 = m->wy + (int)(m->wh * (r + 1) / rows);
		h = y1 - y;
		rem = m->ww;
		sfacts = 0;

		for(i = r; i < n; i += rows) {
			c = cl[i];
			if(c->isshaded)
				rem -= titlebar_width + 2 * c->bw + 2 * (int)gappx;
			else
				sfacts += c->cfact;
		}

		x = m->wx;
		for(i = r; i < n; i += rows) {
			c = cl[i];
			if(c->isshaded) {
				c->shadevertical = 1;
				resizetile(c, x, y, titlebar_width + 2 * (int)gappx,
					h - 2 * c->bw, False);
				x += titlebar_width + 2 * c->bw + 2 * (int)gappx;
			} else {
				cw = sfacts > 0
					? (int)(rem * (c->cfact / sfacts)) : 0;
				resizetile(c, x, y, MAX(1, cw) - 2 * c->bw,
					h - 2 * c->bw, False);
				x += cw;
				rem -= cw;
				sfacts -= c->cfact;
			}
		}
	}

	free(cl);
}

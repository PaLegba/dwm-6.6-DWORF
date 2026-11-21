/* shared factor grid: equal cells by default, cfact weights columns/rows
 * and mfact biases the first column (0.5 = equal) */
static void
factorgrid(Monitor *m, Client **cl, unsigned int n, int x0, int y0,
	int w, int h, int gapped)
{
	unsigned int i, r, c, rows, cols;
	float *colfact, *rowfact, cwsum = 0, rhsum = 0, cum;
	int *colx, *colw, *rowy, *rowh, *coln, *rown, pos, end;

	if (!n)
		return;

	for (rows = 0; rows <= n / 2; rows++)
		if (rows * rows >= n)
			break;
	cols = (rows && (rows - 1) * rows >= n) ? rows - 1 : rows;
	if (rows == 0)
		rows = 1;
	if (cols == 0)
		cols = 1;

	colfact = ecalloc(cols, sizeof(float));
	rowfact = ecalloc(rows, sizeof(float));
	colx = ecalloc(cols, sizeof(int));
	colw = ecalloc(cols, sizeof(int));
	rowy = ecalloc(rows, sizeof(int));
	rowh = ecalloc(rows, sizeof(int));
	coln = ecalloc(cols, sizeof(int));
	rown = ecalloc(rows, sizeof(int));

	/* index layout is column-major: i = row + column * rows */
	for (i = 0; i < n; i++) {
		r = i % rows;
		c = i / rows;
		colfact[c] += cl[i]->cfact;
		rowfact[r] += cl[i]->cfact;
		coln[c]++;
		rown[r]++;
	}
	for (c = 0; c < cols; c++)
		if (coln[c])
			colfact[c] /= coln[c];
	for (r = 0; r < rows; r++)
		if (rown[r])
			rowfact[r] /= rown[r];
	if (cols > 1)
		colfact[0] *= m->mfact / 0.5;

	for (c = 0; c < cols; c++)
		cwsum += colfact[c];
	for (r = 0; r < rows; r++)
		rhsum += rowfact[r];

	pos = 0;
	cum = 0;
	for (c = 0; c < cols; c++) {
		cum += colfact[c];
		end = cwsum > 0 ? (int)(w * cum / cwsum + 0.5) : 0;
		colx[c] = pos;
		colw[c] = end - pos;
		pos = end;
	}
	pos = 0;
	cum = 0;
	for (r = 0; r < rows; r++) {
		cum += rowfact[r];
		end = rhsum > 0 ? (int)(h * cum / rhsum + 0.5) : 0;
		rowy[r] = pos;
		rowh[r] = end - pos;
		pos = end;
	}

	for (i = 0; i < n; i++) {
		Client *cl2 = cl[i];
		unsigned int col = i / rows;
		unsigned int j;
		int cx = x0 + colx[col];
		int cy = y0 + rowy[i % rows];
		int cw = MAX(1, colw[col]);
		int ch = MAX(1, rowh[i % rows]);

		/* the last client in a ragged row spans the remaining columns */
		if (i + rows >= n)
			for (j = col + 1; j < cols; j++)
				cw += colw[j];

		if (gapped)
			resizetile(cl2, cx, cy, cw - 2 * cl2->bw,
				ch - 2 * cl2->bw, 0);
		else
			resize(cl2, cx, cy, cw - 2 * cl2->bw,
				ch - 2 * cl2->bw, False);
	}

	free(colfact);
	free(rowfact);
	free(colx);
	free(colw);
	free(rowy);
	free(rowh);
	free(coln);
	free(rown);
}

void
grid(Monitor *m)
{
	unsigned int i, n, nu;
	Client *c, **cl, **ul;
	int railx, gridw, barw;

	if (!(cl = tiledclients(m, &n)))
		return;

	ul = ecalloc(n, sizeof(Client *));
	for (i = 0, nu = 0; i < n; i++)
		if (!cl[i]->isshaded)
			ul[nu++] = cl[i];

	/* shaded windows dock to the right edge; the rest regrid the
	 * remaining space instead of leaving their cells reserved */
	railx = m->wx + m->ww;
	for (i = 0; i < n; i++) {
		c = cl[i];
		if (!c->isshaded)
			continue;
		barw = titlebar_width + 2 * c->bw + 2 * (int)gappx;
		railx -= barw;
		c->shadevertical = 1;
		resizetile(c, railx, m->wy, titlebar_width + 2 * (int)gappx,
			m->wh - 2 * c->bw, 0);
	}

	gridw = railx - m->wx;
	if (nu > 0 && gridw > 0)
		factorgrid(m, ul, nu, m->wx, m->wy, gridw, m->wh, 1);

	free(ul);
	free(cl);
}

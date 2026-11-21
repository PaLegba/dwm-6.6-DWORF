void
gaplessgrid(Monitor *m)
{
	Client **cl;
	unsigned int n;

	if (!(cl = tiledclients(m, &n)))
		return;

	factorgrid(m, cl, n, m->wx, m->wy, m->ww, m->wh, 0);

	free(cl);
}

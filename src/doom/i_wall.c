void draw_line(int x0, int y0, const int x1, const int y1, const uint32_t color) {
    const int dx = abs(x1 - x0);
    const int dy = abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    const int err = (dx > dy ? dx : -dy) / 2;

    while (1) {
        // only draw pixels when shown on screen
        if (x0 >= 0 && x0 < SCREEN_WIDTH && y0 >= 0 && y0 < SCREEN_HEIGHT) {
            state.pixels[y0 * SCREEN_WIDTH + x0] = color;
        }

        if (x0 == x1 && y0 == y1) break;

        const int e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy) {
            err += dx;
            y0 += sy;
        }
    }
}
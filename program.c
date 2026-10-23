#include <stdio.h>

#define WIDTH  60
#define HEIGHT 30

/* Tableau statique : HEIGHT lignes x WIDTH colonnes, initialisé à 0 (cellules mortes) */
static int world[HEIGHT][WIDTH] = {0};

/* Place un motif (tableau de coordonnées relatives) à la position (x, y) */
static void place(const int pattern[][2], int n, int x, int y)
{
    for (int i = 0; i < n; i++)
        world[y + pattern[i][1]][x + pattern[i][0]] = 1;
}

static void init_world(void)
{
    /* Structure stable : bloc 2x2 */
    const int block[][2] = { {0,0}, {1,0}, {0,1}, {1,1} };

    /* Oscillateur : clignotant (blinker), période 2 */
    const int blinker[][2] = { {0,0}, {1,0}, {2,0} };

    /* Vaisseau : planeur (glider) */
    const int glider[][2] = { {1,0}, {2,1}, {0,2}, {1,2}, {2,2} };

    place(block,   4, 10, 10);
    place(blinker, 3, 30, 10);
    place(glider,  5, 50, 10);
}

static void display_world(void)
{
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++)
            putchar(world[y][x] ? '#' : '.');
        putchar('\n');
    }
}

int main(void)
{
    init_world();
    display_world();
    return 0;
}
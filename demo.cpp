#include <go32.h>
#include <sys/farptr.h>
#include <conio.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include <bios.h>
#include <algorithm>
#include <array>
#include <random>
#include <iostream>
#include <time.h>
#include <unistd.h>
#include <memory>
#include <fstream>
#include "NativeBitmap.h"
#include "LoadImage.h"

std::vector<std::shared_ptr<odb::NativeBitmap>> tiles;

std::shared_ptr<odb::NativeBitmap> hero = odb::loadBitmap("hero.png");

int tilesRoom[6][10];

int px = 0;
int py = 0;
int vx = 0;
int vy = 0;
int counter = 0;
std::array<unsigned int, 320 * 200> imageBuffer;
std::array<unsigned char, 320 * 200> buffer;

void initMode13h() {
    union REGS regs;

    regs.h.ah = 0x00;
    regs.h.al = 0x4;
    int86(0x10, &regs, &regs);
}

void plot(int x, int y, int color) {
    int b, m; /* bits and mask */
    unsigned char *p;
    unsigned char c;
    _farsetsel( _dos_ds );
    /* address section differs depending on odd/even scanline */
    if (1 == (y & 0x1)) {
        p = (unsigned char *)(0xB800 * 16) + 0x2000;
    } else {
        p = (unsigned char *)(0xB800 * 16);
    }

    /* divide by 2 (each address section is 100 pixels) */
    y >>= 1;

    /* start bit (b) and mask (m) for 2-bit pixels */
    switch (x & 0x3) {
        case 0:
            b = 6;
            m = 0xC0;
            break;
        case 1:
            b = 4;
            m = 0x30;
            break;
        case 2:
            b = 2;
            m = 0x0C;
            break;
        case 3:
            b = 0;
            m = 0x03;
            break;
    }

    /* divide X by 4 (2 bits for each pixel) */
    x >>= 2;

    unsigned int offset = ((80 * y) + x);

    /* 80 bytes per line (80 * 4 = 320), 4 pixels per byte */
    p += offset;

    /* read current pixel */
    c = _farnspeekb( p );

    /* remove bits at new position */
    c = c & ~m;

    /* set bits at new position */
    c = c | (color << b);

    /* write new pixel */
    _farnspokeb( p, c );
}

void copyImageBufferToVideoMemory() {

    int origin = 0;
    int offset = 0;
    int value = 0;
    for (int y = 0; y < 200; ++y) {

        if (y < 0 || y >= 200) {
            continue;
        }

        for (int x = 0; x < 320; ++x) {

            if (x < 0 || x >= 320) {
                continue;
            }

            offset = (y * 320) + x;
            origin = imageBuffer[offset];

            value = origin;

            if (0 < origin && origin < 4) {
                if (((x + y) % 2) == 0) {
                    value = 0;
                } else {
                    value = origin;
                }
            }

            if (4 <= origin && origin < 8) {
                value = origin - 4;
            }

            if (origin >= 8) {
                if (((x + y) % 2) == 0) {
                    value = 3;
                } else {
                    value = origin - 8;
                }
            }

            if (buffer[offset] != origin ) {
                plot( x, y, value );
            }

            buffer[offset] = origin;
        }
    }
}

void render() {
    std::fill(std::begin(imageBuffer), std::end(imageBuffer), 4);

    int y0 = 0;
    int y1 = 0;
    int x0 = 0;
    int x1 = 0;


    for (int ty = 0; ty < 6; ++ty) {
        for (int tx = 0; tx < 10; ++tx) {
            std::shared_ptr<odb::NativeBitmap> tile = tiles[tilesRoom[ty][tx]];

            if ( tile == nullptr ) {
                std::cout <<  "null tile at " << tx << ", " << ty << std::endl;
                exit(0);
            }

            y0 = (ty * 32);
            y1 = 32 + (ty * 32);
            x0 = (tx * 32);
            x1 = 32 + (tx * 32);
            int *pixelData = tile->getPixelData();

            int pixel = 4;
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {

                    pixel = (pixelData[(32 * (y - y0)) + (x - x0)]);

                    if ( pixel == 0 ) {
                        continue;
                    }

                    imageBuffer[(320 * y) + x] = pixel;
                }
            }


        }
    }

    y0 = (py );
    y1 = 32 + y0;
    x0 = (px);
    x1 = 32 + x0;
    int *pixelData = hero->getPixelData();

    int pixel = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            pixel = (pixelData[(32 * (y - y0)) + (x - x0)]);
            if ( pixel == 0 ) {
                continue;
            }
            imageBuffer[(320 * y) + x] = pixel;
        }
    }

    copyImageBufferToVideoMemory();
    usleep(20000);
}

int main(int argc, char **argv) {

    std::ifstream tilemap("tilemap");

    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 10; ++x) {
            char ch = '0';

            tilemap >> ch;
            tilesRoom[y][x] = ch - '0';
        }
    }

    std::ifstream tileList("tilelist");
    std::string buffer;

    while ( tileList.good() ) {
        std::getline( tileList, buffer );
        std::cout << "tile: " << buffer << std::endl;
        tiles.push_back(odb::loadBitmap(buffer));
    }

    bool done = false;

    char lastKey = 0;

    initMode13h();

    while (!done) {

        px += vx;
        py += vy;

        vx = vy = 0;

        if (px < 0) {
            px = 0;
        }

        if (py < 0) {
            py = 0;
        }

        if (px >= 315) {
            px = 315;
        }

        if (py >= 195) {
            py = 195;
        }

        int level = 0;
        ++counter;
        render();

        while (kbhit()) {
            lastKey = getch();
            switch (lastKey) {
                case 'q':
                    done = true;
                    break;
                case 'w':
                    vy = -5;
                    break;
                case 's':
                    vy = +5;
                    break;
                case 'a':
                    vx = -5;
                    break;
                case 'd':
                    vx = +5;
                    break;
            }
        }
    }

    return 0;
}
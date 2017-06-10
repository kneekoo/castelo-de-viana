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
#include <sstream>
#include "NativeBitmap.h"
#include "LoadImage.h"

#include "Game.h"


std::vector<std::vector<std::shared_ptr<odb::NativeBitmap>>> tiles;

std::shared_ptr<odb::NativeBitmap> arrowSprite = odb::loadBitmap("arrow.png");

std::shared_ptr<odb::NativeBitmap> doorStates[2] = {
        odb::loadBitmap("door0.png"),
        odb::loadBitmap("door1.png"),
};

std::shared_ptr<odb::NativeBitmap> foeSprites[2] = {
        odb::loadBitmap("foe0.png"),
        odb::loadBitmap("foe1.png"),
};

std::shared_ptr<odb::NativeBitmap> itemSprites[2] = {
        odb::loadBitmap("meat.png"),
        odb::loadBitmap("key.png"),
};

std::shared_ptr<odb::NativeBitmap> hero[6][2] = {
        {
                odb::loadBitmap("up0.png"),
                odb::loadBitmap("up1.png"),
        },
        {
                odb::loadBitmap("hero0.png"),
                odb::loadBitmap("hero1.png"),
        },
        {
                odb::loadBitmap("down0.png"),
                odb::loadBitmap("down1.png"),
        },
        {
                odb::loadBitmap("attack0.png"),
                odb::loadBitmap("attack0.png"),
        },
        {
                odb::loadBitmap("jump0.png"),
                odb::loadBitmap("jump0.png"),
        },
        {
                odb::loadBitmap("up0.png"),
                odb::loadBitmap("up1.png"),
        },
};

std::array<unsigned int, 320 * 200> imageBuffer;
std::array<unsigned char, 320 * 200> buffer;
std::array<unsigned char, 320 * 100 / 4> evenBuffer;
std::array<unsigned char, 320 * 100 / 4> oddBuffer;


void initMode4h() {
    union REGS regs;

    regs.h.ah = 0x00;
    regs.h.al = 0x4;
    int86(0x10, &regs, &regs);
}

void clearBuffers() {
    std::fill(std::begin(imageBuffer), std::end(imageBuffer), 4);
    std::fill(std::begin(buffer), std::end(buffer), 4);
    std::fill(std::begin(evenBuffer), std::end(evenBuffer), 0);
    std::fill(std::begin(oddBuffer), std::end(oddBuffer), 0);
}

std::vector<std::shared_ptr<odb::NativeBitmap>> loadSpriteList(std::string listName ) {
    std::ifstream tileList(listName);
    std::string buffer;

    std::vector<std::shared_ptr<odb::NativeBitmap>> tilesToLoad;

    while (tileList.good()) {
        std::getline(tileList, buffer);
        tilesToLoad.push_back(odb::loadBitmap(buffer));
    }
    return tilesToLoad;
}

void loadTiles( std::vector<std::string> tilesToLoad ) {
    tiles.clear();

    for (const auto& tile : tilesToLoad ) {

        if ( tile.substr( tile.length() - 4 ) == ".png" ) {
            tiles.push_back({odb::loadBitmap(tile)});
        } else {
            tiles.push_back(loadSpriteList(tile));
        }
    }
}

void plot(int x, int y, int color) {
    int b, m; /* bits and mask */
    unsigned char c;
    /* address section differs depending on odd/even scanline */
    bool odd = (1 == (y & 0x1));

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

    /* read current pixel */
    if (odd) {
        c = oddBuffer[offset];
    } else {
        c = evenBuffer[offset];
    }

    /* remove bits at new position */
    c = c & ~m;

    /* set bits at new position */
    c = c | (color << b);

    if (odd) {
        oddBuffer[offset] = c;
    } else {
        evenBuffer[offset] = c;
    }
}

void copyImageBufferToVideoMemory() {

    int origin = 0;
    int value = 0;
    int last = 0;
    auto currentImageBufferPos = std::begin(imageBuffer);
    auto currentBufferPos = std::begin(buffer);

    for (int y = 0; y < 200; ++y) {

        if (y < 0 || y >= 200) {
            continue;
        }

        for (int x = 0; x < 320; ++x) {

            if (x < 0 || x >= 320) {
                continue;
            }

            origin = *currentImageBufferPos;
            last = *currentBufferPos;

            if (last == origin) {
                currentBufferPos = std::next(currentBufferPos);
                currentImageBufferPos = std::next(currentImageBufferPos);
                continue;
            }

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

            plot(x, y, value);
            *currentBufferPos = origin;

            currentBufferPos = std::next(currentBufferPos);
            currentImageBufferPos = std::next(currentImageBufferPos);
        }
    }

    dosmemput(evenBuffer.data(), 320 * 100 / 4, 0xB800 * 16);
    dosmemput(oddBuffer.data(), 320 * 100 / 4, (0xB800 * 16) + 0x2000);

    //gotoxy(1, 1);
    //std::cout << "room " << room << std::endl;
}

void render() {
    std::fill(std::begin(imageBuffer), std::end(imageBuffer), 4);

    int y0 = 0;
    int y1 = 0;
    int x0 = 0;
    int x1 = 0;


    for (int ty = 0; ty < 6; ++ty) {
        for (int tx = 0; tx < 10; ++tx) {
            std::shared_ptr<odb::NativeBitmap> tile;
            int *pixelData;
            y0 = (ty * 32);
            y1 = 32 + (ty * 32);
            x0 = (tx * 32);
            x1 = 32 + (tx * 32);
            int pixel = 4;

            if (backgroundTiles[ty][tx] != 0) {
                auto tileset = tiles[backgroundTiles[ty][tx]];
                tile = tileset[ counter % tileset.size() ];

                if (tile == nullptr) {
                    std::cout << "null tile at " << tx << ", " << ty << std::endl;
                    exit(0);
                }

                pixelData = tile->getPixelData();

                pixel = 4;
                for (int y = y0; y < y1; ++y) {
                    if (y < 0 || y >= 200) {
                        continue;
                    }

                    for (int x = x0; x < x1; ++x) {

                        if (x < 0 || x >= 320) {
                            continue;
                        }

                        pixel = (pixelData[(32 * (y - y0)) + (x - x0)]);

                        if (pixel == 0) {
                            continue;
                        }

                        imageBuffer[(320 * y) + x] = pixel;
                    }
                }
            }

            if (foregroundTiles[ty][tx] != 0) {
                auto tileset = tiles[foregroundTiles[ty][tx]];
                tile = tileset[ counter % tileset.size() ];


                if (tile == nullptr) {
                    std::cout << "null tile at " << tx << ", " << ty << std::endl;
                    exit(0);
                }

                pixelData = tile->getPixelData();

                pixel = 4;
                for (int y = y0; y < y1; ++y) {

                    if (y < 0 || y >= 200) {
                        continue;
                    }

                    for (int x = x0; x < x1; ++x) {

                        if (x < 0 || x >= 320) {
                            continue;
                        }

                        pixel = (pixelData[(32 * (y - y0)) + (x - x0)]);

                        if (pixel == 0) {
                            continue;
                        }

                        imageBuffer[(320 * y) + x] = pixel;
                    }
                }
            }
        }
    }

    int *pixelData;

    for ( const auto& door : doors ) {
        pixelData = doorStates[ door.mType - EActorType::kClosedDoor ]->getPixelData();
        y0 = (door.mPosition.mY);
        y1 = 32 + y0;
        x0 = (door.mPosition.mX);
        x1 = 32 + x0;

        int pixel = 0;
        for (int y = y0; y < y1; ++y) {

            if (y < 0 || y >= 200) {
                continue;
            }

            for (int x = x0; x < x1; ++x) {
                pixel = (pixelData[(32 * (y - y0)) + ((x - x0))]);

                if (pixel == 0) {
                    continue;
                }

                if (x < 0 || x >= 320) {
                    continue;
                }

                imageBuffer[(320 * y) + (x)] = pixel;
            }
        }
    }

    auto sprite = hero[player.mStance][heroFrame];

    if ( (ticksUntilVulnerable <= 0 ) || (( counter % 2) == 0) ) {
        y0 = (player.mPosition.mY);
        int spriteWidth = sprite->getWidth();
        y1 = sprite->getHeight() + y0;
        x0 = (player.mPosition.mX);

        if (player.mDirection == EDirection::kLeft) {
            x0 -= (spriteWidth - 32);
        }

        x1 = spriteWidth + x0;
        pixelData = sprite->getPixelData();

        int pixel = 0;
        for (int y = y0; y < y1; ++y) {

            if (y < 0 || y >= 200) {
                continue;
            }

            for (int x = x0; x < x1; ++x) {
                if (player.mDirection == EDirection::kRight) {
                    pixel = (pixelData[(spriteWidth* (y - y0)) + ((x - x0))]);
                } else {
                    pixel = (pixelData[(spriteWidth * (y - y0)) + ( ( spriteWidth - 1 ) - (x - x0))]);
                }

                if (pixel == 0) {
                    continue;
                }

                if (x < 0 || x >= 320) {
                    continue;
                }

                imageBuffer[(320 * y) + (x)] = pixel;
            }
        }
    }

    pixelData = arrowSprite->getPixelData();

    for ( const auto& arrow : arrows ) {
        y0 = (arrow.mPosition.mY);
        y1 = 32 + y0;
        x0 = (arrow.mPosition.mX);
        x1 = 32 + x0;

        int pixel = 0;
        for (int y = y0; y < y1; ++y) {

            if (y < 0 || y >= 200) {
                continue;
            }

            for (int x = x0; x < x1; ++x) {
                if (arrow.mDirection == EDirection::kRight) {
                    pixel = (pixelData[(32 * (y - y0)) + ((x - x0))]);
                } else {
                    pixel = (pixelData[(32 * (y - y0)) + (31 - (x - x0))]);
                }


                if (pixel == 0) {
                    continue;
                }

                if (x < 0 || x >= 320) {
                    continue;
                }

                imageBuffer[(320 * y) + (x)] = pixel;
            }
        }

    }

    pixelData = foeSprites[counter % 2 ]->getPixelData();

    for ( const auto& foe : foes ) {

        if ( foe.mType != EActorType::kSkeleton ) {
            continue;
        }

        y0 = (foe.mPosition.mY);
        y1 = 32 + y0;
        x0 = (foe.mPosition.mX);
        x1 = 32 + x0;

        int pixel = 0;
        for (int y = y0; y < y1; ++y) {

            if (y < 0 || y >= 200) {
                continue;
            }

            for (int x = x0; x < x1; ++x) {
                if (foe.mDirection == EDirection::kRight) {
                    pixel = (pixelData[(32 * (y - y0)) + ((x - x0))]);
                } else {
                    pixel = (pixelData[(32 * (y - y0)) + (31 - (x - x0))]);
                }


                if (pixel == 0) {
                    continue;
                }

                if (x < 0 || x >= 320) {
                    continue;
                }

                imageBuffer[(320 * y) + (x)] = pixel;
            }
        }
    }

    for ( const auto& item : items) {
        y0 = (item.mPosition.mY);
        y1 = 32 + y0;
        x0 = (item.mPosition.mX);
        x1 = 32 + x0;
        pixelData = itemSprites[ item.mType ]->getPixelData();
        int pixel = 0;
        for (int y = y0; y < y1; ++y) {

            if (y < 0 || y >= 200) {
                continue;
            }

            for (int x = x0; x < x1; ++x) {
                pixel = (pixelData[(32 * (y - y0)) + ((x - x0))]);

                if (pixel == 0) {
                    continue;
                }

                if (x < 0 || x >= 320) {
                    continue;
                }

                imageBuffer[(320 * y) + (x)] = pixel;
            }
        }
    }

    if ( hasKey && (( counter % 2) == 0) ) {
        y0 = 2;
        y1 = 32 + y0;
        x0 = 2;
        x1 = 32 + x0;
        pixelData = itemSprites[ kKey ]->getPixelData();
        int pixel = 0;
        for (int y = y0; y < y1; ++y) {

            if (y < 0 || y >= 200) {
                continue;
            }

            for (int x = x0; x < x1; ++x) {
                pixel = (pixelData[(32 * (y - y0)) + ((x - x0))]);

                if (pixel == 0) {
                    continue;
                }

                if (x < 0 || x >= 320) {
                    continue;
                }

                imageBuffer[(320 * y) + (x)] = pixel;
            }
        }
    }

    copyImageBufferToVideoMemory();
    if ( ticksToShowHealth > 0 ) {
        gotoxy(1, 24);
        std::cout << "PLAYER: ";

        for (int c = 0; c < 10; ++c) {
            char ch;
            if (c >= player.mHealth) {
                ch = 176;
            } else {
                ch = 219;
            }

            std::cout << ch;
        }

        std::cout << std::endl;
    }
    usleep(20000);
}


int main(int argc, char **argv) {
    init();
    prepareRoom(0);

    bool done = false;

    int lastKey = 0;

    initMode4h();

    while (!done) {

        bool isOnGround = false;
        bool isJumping = false;
        bool isUpPressed = false;
        bool isDownPressed = false;
        bool isLeftPressed = false;
        bool isRightPressed = false;
        bool isAttacking = false;
        bool isAltAttackPressed = false;

        bool isOnStairs;
        render();
        gameTick(isOnGround, isOnStairs);

        lastKey = bioskey(0x11);
        auto extendedKeys = bioskey(0x12);

        if (extendedKeys & (0b0000000000000100) ||
            extendedKeys & (0b0000000100000000)
                ) {
            isAttacking = true;
        }

        if (extendedKeys & (0b0000000000000001) ||
            extendedKeys & (0b0000000000000010)
                ) {
            isJumping = true;
        }

        bdos(0xC, 0, 0);
        //gotoxy(1, 2);

        //std::cout << "key: " << lastKey << std::endl;

        switch (lastKey) {
            case 27:
            case 283:
                done = true;
                break;
            case 'q':
                isJumping = true;
                break;
            case 'w':
            case 4471:
            case 18656:
                isUpPressed = true;
                break;
            case 's':
            case 8051:
            case 20704:
                isDownPressed = true;
                break;
            case 'a':
            case 7777:
            case 19424:
                isLeftPressed = true;
                break;
            case 'd':
            case 8292:
            case 19936:
                isRightPressed = true;
                break;
            case ' ':
            case 14624:
                isAltAttackPressed = true;
        }


        updateHero(isOnGround, isJumping, isUpPressed, isDownPressed, isLeftPressed, isAttacking, isAltAttackPressed, isRightPressed, isOnStairs);

    }

    nosound();
    textmode(C80);
    clrscr();
    std::cout << "Thanks for playing!" << std::endl;

    return 0;
}
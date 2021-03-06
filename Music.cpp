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
#include <string>
#include <vector>
#include <EASTL/vector.h>
#include <EASTL/array.h>

using std::string;
using eastl::array;
using eastl::vector;

#include "NativeBitmap.h"
#include "LoadImage.h"
#include "Game.h"

void playMusic(const string &music) {
    int frequency = 0;
    melody.clear();
    for (const auto &note : music) {
        switch (note) {
            case 'a':
            case 'A':
                frequency = 932;
                break;
            case 'B':
            case 'b':
                frequency = 988;
                break;
            case 'C':
            case 'c':
                frequency = 1109;
                break;
            case 'D':
            case 'd':
                frequency = 1175;
                break;
            case 'E':
            case 'e':
                frequency = 1318;
                break;
            case 'F':
            case 'f':
                frequency = 1397;
                break;
            case 'G':
            case 'g':
                frequency = 1568;
                break;
        }
        melody.push_back(frequency);
    }
    playSound(melody);
}
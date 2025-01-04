


#include <iostream>
#include <atomic>
#include <termios.h>
#include <unistd.h>
#include <cmath>
#include <chrono>
#include "oldNoiseMaker.h"

#define PI 3.14159265358979323846 // you wouldn't believe me if i told you, i wrote this myself in one go, yea

std::atomic<double> frequency{0.0};
std::atomic<bool> sustain{false};
std::chrono::steady_clock::time_point lastKeyPressTime;

void setupKeyboard() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

char getKey() {
    char c = 0;
    read(STDIN_FILENO, &c, 1);
    return c;
}

int main() {
    setupKeyboard();
    
    LinuxNoiseMaker<int16_t> sound("default", 44100, 1, 8, 512);
    
    sound.SetUserFunction([](double time) -> double {
        if (frequency == 0.0) return 0.0;
        return sin(frequency * 2.0 * PI * time) * 0.5;
    });

    std::cout << "Piano Keys: A S D F G H J K L ;\n";
    std::cout << "Press Q to quit\n";

    const double notes[] = {
        440.00,
        493.88,
        523.25,
        587.33,
        659.25,
        698.46,
        783.99,
        880.00,
        987.77,
        1046.50
    };

    const int sustainDuration = 500;

    while (true) {
        char key = getKey();
        
        if (key != 0) {
            key = tolower(key);
            
            if (key == 'q') break;
            
            switch(key) {
                case 'a': frequency = notes[0]; break;
                case 's': frequency = notes[1]; break;
                case 'd': frequency = notes[2]; break;
                case 'f': frequency = notes[3]; break;
                case 'g': frequency = notes[4]; break;
                case 'h': frequency = notes[5]; break;
                case 'j': frequency = notes[6]; break;
                case 'k': frequency = notes[7]; break;
                case 'l': frequency = notes[8]; break;
                case ';': frequency = notes[9]; break;
                default: frequency = 0.0;
            }
            
            if (frequency > 0) {
                sustain = true;
                lastKeyPressTime = std::chrono::steady_clock::now();
                std::cout << "\rPlaying: " << frequency << "Hz     ";
                std::cout.flush();
            }
        } else {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastKeyPressTime).count();
            
            if (sustain && elapsed >= sustainDuration) {
                frequency = 0.0;
                sustain = false;
                std::cout << "\rStopped           ";
                std::cout.flush();
            }
        }
        
        usleep(1000);
    }

    return 0;
}

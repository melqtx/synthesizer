
#include <iostream>
#include <atomic>
#include <termios.h>
#include <unistd.h>
#include <cmath>
#include <chrono>
#include <map>
#include <vector>
#include "oldNoiseMaker.h"
#define PI 3.14159265358979323846

struct Note {
    double frequency;
    double startTime;
    bool isActive;
    double velocity;
};

std::map<char, Note> activeNotes;
std::atomic<bool> sustain{false};

double envelope(double time, double startTime) {
    double elapsed = time - startTime;
    
    const double attackTime = 0.01;
    const double decayTime = 0.1;
    const double sustainLevel = 0.7;
    const double releaseTime = 0.3;
    
    if (elapsed < attackTime) {
        return elapsed / attackTime;
    }
    else if (elapsed < attackTime + decayTime) {
        double decayProgress = (elapsed - attackTime) / decayTime;
        return 1.0 - (1.0 - sustainLevel) * decayProgress;
    }
    else if (sustain) {
        return sustainLevel;
    }
    else {
        double releaseStart = attackTime + decayTime;
        double releaseProgress = (elapsed - releaseStart) / releaseTime;
        return sustainLevel * (1.0 - std::min(1.0, releaseProgress));
    }
}
// make it more pianosh
double pianoWave(double time, double freq) {
    double fundamental = sin(freq * 2.0 * PI * time);
    double second = 0.5 * sin(2 * freq * 2.0 * PI * time);
    double third = 0.25 * sin(3 * freq * 2.0 * PI * time);
    double fourth = 0.125 * sin(4 * freq * 2.0 * PI * time);
    
    double detune1 = 0.1 * sin((freq * 1.001) * 2.0 * PI * time);
    double detune2 = 0.1 * sin((freq * 0.999) * 2.0 * PI * time);
    
    return (fundamental + second + third + fourth + detune1 + detune2) / 4.0;
}

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
        double mixedOutput = 0.0;
        // mixing the notes
        for (const auto& [key, note] : activeNotes) {
            if (note.isActive) {
                double env = envelope(time, note.startTime);
                double wave = pianoWave(time, note.frequency);
                mixedOutput += wave * env * note.velocity;
            }
        }
        
        int activeCount = 0;
        for (const auto& [key, note] : activeNotes) {
            if (note.isActive) activeCount++;
        }
        
        if (activeCount > 0) {
            mixedOutput = mixedOutput / activeCount * 0.5;
        }
        
        return mixedOutput;
    });

    std::cout << "Piano Keys: A S D F G H J K L ;\n";
    std::cout << "Press Q to quit\n";
    
    const std::map<char, double> noteFrequencies = {
        {'a', 440.00},  // A4
        {'s', 493.88},  // B4
        {'d', 523.25},  // C5
        {'f', 587.33},  // D5
        {'g', 659.25},  // E5
        {'h', 698.46},  // F5
        {'j', 783.99},  // G5
        {'k', 880.00},  // A5
        {'l', 987.77},  // B5
        {';', 1046.50}  // C6
    };

    while (true) {
        char key = getKey();
        
        if (key != 0) {
            key = tolower(key);
            
            if (key == 'q') break;
            
            auto it = noteFrequencies.find(key);
            if (it != noteFrequencies.end()) {
                Note newNote = {
                    it->second,          
                    sound.GetTime(),      
                    true,              
                    0.7 
                };
                
                activeNotes[key] = newNote;
                
                std::cout << "\rPlaying: ";
                for (const auto& [k, note] : activeNotes) {
                    if (note.isActive) {
                        std::cout << note.frequency << "Hz ";
                    }
                }
                std::cout << "     ";
                std::cout.flush();
            }
        } else {
            for (auto& [k, note] : activeNotes) {
                if (note.isActive) {
                    if (sound.GetTime() - note.startTime > 0.5) {
                        note.isActive = false;
                    }
                }
            }
        }
        
        usleep(1000);
    }
    
    return 0;
}


g++ main.cpp -o main -lasound -pthread

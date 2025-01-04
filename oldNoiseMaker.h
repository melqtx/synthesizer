
#pragma once

#include <alsa/asoundlib.h>
#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

const double PI = 2.0 * acos(0.0);

template<class T>
class LinuxNoiseMaker {
public:
    LinuxNoiseMaker(std::string sOutputDevice = "default", unsigned int nSampleRate = 44100, 
                    unsigned int nChannels = 1, unsigned int nBlocks = 8, unsigned int nBlockSamples = 512) {
        Create(sOutputDevice, nSampleRate, nChannels, nBlocks, nBlockSamples);
    }

    ~LinuxNoiseMaker() {
        Destroy();
    }

    bool Create(std::string sOutputDevice, unsigned int nSampleRate = 44100, unsigned int nChannels = 1, 
                unsigned int nBlocks = 8, unsigned int nBlockSamples = 512) {
        m_bReady = false;
        m_nSampleRate = nSampleRate;
        m_nChannels = nChannels;
        m_nBlockCount = nBlocks;
        m_nBlockSamples = nBlockSamples;
        m_nBlockFree = m_nBlockCount;
        m_nBlockCurrent = 0;

        // Open PCM device
        int rc = snd_pcm_open(&m_handle, sOutputDevice.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
        if (rc < 0) {
            std::cerr << "Cannot open audio device: " << snd_strerror(rc) << std::endl;
            return false;
        }

        // Allocate hardware parameters object
        snd_pcm_hw_params_t *params;
        snd_pcm_hw_params_alloca(&params);

        // Fill it with default values
        snd_pcm_hw_params_any(m_handle, params);

        // Set parameters
        rc = snd_pcm_hw_params_set_access(m_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
        if (rc < 0) {
            std::cerr << "Cannot set interleaved mode: " << snd_strerror(rc) << std::endl;
            return false;
        }

        // Signed 16-bit little-endian format
        if (sizeof(T) == 2)
            rc = snd_pcm_hw_params_set_format(m_handle, params, SND_PCM_FORMAT_S16_LE);
        else
            rc = snd_pcm_hw_params_set_format(m_handle, params, SND_PCM_FORMAT_FLOAT);

        if (rc < 0) {
            std::cerr << "Cannot set format: " << snd_strerror(rc) << std::endl;
            return false;
        }

        // Set channels
        rc = snd_pcm_hw_params_set_channels(m_handle, params, m_nChannels);
        if (rc < 0) {
            std::cerr << "Cannot set channels: " << snd_strerror(rc) << std::endl;
            return false;
        }

        // Set sample rate
        unsigned int actual_rate = m_nSampleRate;
        rc = snd_pcm_hw_params_set_rate_near(m_handle, params, &actual_rate, 0);
        if (rc < 0) {
            std::cerr << "Cannot set sample rate: " << snd_strerror(rc) << std::endl;
            return false;
        }

        // Apply hardware parameters
        rc = snd_pcm_hw_params(m_handle, params);
        if (rc < 0) {
            std::cerr << "Cannot set parameters: " << snd_strerror(rc) << std::endl;
            return false;
        }

        // Allocate buffer memory
        m_pBlockMemory = new T[m_nBlockCount * m_nBlockSamples];
        if (m_pBlockMemory == nullptr)
            return false;

        memset(m_pBlockMemory, 0, sizeof(T) * m_nBlockCount * m_nBlockSamples);

        m_bReady = true;
        m_thread = std::thread(&LinuxNoiseMaker::MainThread, this);
        return true;
    }

    bool Destroy() {
        if (m_handle) {
            snd_pcm_drain(m_handle);
            snd_pcm_close(m_handle);
        }
        delete[] m_pBlockMemory;
        return true;
    }

    void Stop() {
        m_bReady = false;
        m_thread.join();
    }

    // Override to process current sample
    virtual double UserProcess(double dTime) {
        return 0.0;
    }

    double GetTime() {
        return m_dGlobalTime;
    }

    void SetUserFunction(double(*func)(double)) {
        m_userFunction = func;
    }

    double clip(double dSample, double dMax) {
        if (dSample >= 0.0)
            return fmin(dSample, dMax);
        else
            return fmax(dSample, -dMax);
    }

private:
    double(*m_userFunction)(double) = nullptr;
    snd_pcm_t *m_handle = nullptr;
    unsigned int m_nSampleRate;
    unsigned int m_nChannels;
    unsigned int m_nBlockCount;
    unsigned int m_nBlockSamples;
    unsigned int m_nBlockCurrent;

    T* m_pBlockMemory;
    std::thread m_thread;
    std::atomic<bool> m_bReady;
    std::atomic<unsigned int> m_nBlockFree;
    std::atomic<double> m_dGlobalTime;

    void MainThread() {
        m_dGlobalTime = 0.0;
        double dTimeStep = 1.0 / (double)m_nSampleRate;

        T nMaxSample = (T)pow(2, (sizeof(T) * 8) - 1) - 1;
        double dMaxSample = (double)nMaxSample;

        while (m_bReady) {
            int nCurrentBlock = m_nBlockCurrent * m_nBlockSamples;
            
            for (unsigned int n = 0; n < m_nBlockSamples; n++) {
                double dOutput = 0.0;
                
                if (m_userFunction == nullptr)
                    dOutput = UserProcess(m_dGlobalTime);
                else
                    dOutput = m_userFunction(m_dGlobalTime);

                T nNewSample = (T)(clip(dOutput, 1.0) * dMaxSample);
                m_pBlockMemory[nCurrentBlock + n] = nNewSample;
                m_dGlobalTime = m_dGlobalTime + dTimeStep;
            }

            // Write block to sound device
            snd_pcm_sframes_t frames = snd_pcm_writei(m_handle, 
                                                     &m_pBlockMemory[nCurrentBlock], 
                                                     m_nBlockSamples);

            if (frames < 0)
                frames = snd_pcm_recover(m_handle, frames, 0);
                
            if (frames < 0) {
                std::cerr << "snd_pcm_writei failed: " << snd_strerror(frames) << std::endl;
                break;
            }

            m_nBlockCurrent++;
            m_nBlockCurrent %= m_nBlockCount;
        }
    }
};

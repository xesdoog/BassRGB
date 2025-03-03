#include <iostream>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <vector>
#include "lpf.h"

#pragma comment(lib, "Avrt.lib")

int main() {
    HRESULT hr;
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioClient* pAudioClient = nullptr;
    IAudioCaptureClient* pCaptureClient = nullptr;

    CoInitialize(nullptr);

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        std::cerr << "Failed to create device enumerator!" << std::endl;
        return -1;
    }

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) {
        std::cerr << "Failed to get default audio device!" << std::endl;
        return -1;
    }

    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hr)) {
        std::cerr << "Failed to activate audio client!" << std::endl;
        return -1;
    }

    WAVEFORMATEX* pWaveFormat;
    hr = pAudioClient->GetMixFormat(&pWaveFormat);
    if (FAILED(hr)) {
        std::cerr << "Failed to get mix format!" << std::endl;
        return -1;
    }

    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
        10000000, 0, pWaveFormat, NULL);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize audio client!" << std::endl;
        return -1;
    }

    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    if (FAILED(hr)) {
        std::cerr << "Failed to get capture client!" << std::endl;
        return -1;
    }

    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        std::cerr << "Failed to start audio capture!" << std::endl;
        return -1;
    }

    std::cout << "Capturing audio... Press Ctrl+C to stop." << std::endl;

    double sampleRate = pWaveFormat->nSamplesPerSec;
    double cutoffFreq = 50.0;
    LowPassFilter lpFilter(sampleRate, cutoffFreq);

    while (true) {
        UINT32 packetLength = 0;
        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) {
            std::cerr << "Failed to get packet size!" << std::endl;
            break;
        }

        while (packetLength > 0) {
            BYTE* pData;
            UINT32 numFramesAvailable;
            DWORD flags;

            hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
            if (FAILED(hr)) {
                std::cerr << "Failed to get buffer!" << std::endl;
                break;
            }

            short* buffer = (short*)pData;
            UINT32 limit = (numFramesAvailable < 10) ? numFramesAvailable : 10;
            for (UINT32 i = 0; i < limit; i++) {
                double sample = static_cast<double>(buffer[i]) / 32768.0;
                double filteredSample = lpFilter.process(sample);

                int barLength = static_cast<int>(std::abs(filteredSample * 32768.0) / 500);
                std::cout << "\rBass: " << std::string(barLength, '|') << std::flush;

                // std::cout << "\rRaw: " << buffer[i] << "  Filtered: " << filteredSample << "     " << std::flush;
            }

            hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
            if (FAILED(hr)) {
                std::cerr << "Failed to release buffer!" << std::endl;
                break;
            }

            hr = pCaptureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) {
                std::cerr << "Failed to get next packet size!" << std::endl;
                break;
            }
        }

        Sleep(100);
    }

    pAudioClient->Stop();
    pCaptureClient->Release();
    pAudioClient->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();

    return 0;
}

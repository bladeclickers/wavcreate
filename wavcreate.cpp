#include "MidiFile.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <Windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

using i16 = int16_t;
using u32 = uint32_t;
using f32 = float;

constexpr u32 FREQ = 44100;

void write_le_16(std::vector<char>& buf, i16 n) {
    buf.push_back(n & 0xFF);
    buf.push_back((n >> 8) & 0xFF);
}

void write_le_32(std::vector<char>& buf, u32 n) {
    buf.push_back(n & 0xFF);
    buf.push_back((n >> 8) & 0xFF);
    buf.push_back((n >> 16) & 0xFF);
    buf.push_back((n >> 24) & 0xFF);
}

void write_wav_header(std::vector<char>& buf, u32 num_samples) {
    u32 data_size = num_samples * sizeof(i16);

    buf.insert(buf.end(), { 'R','I','F','F' });
    write_le_32(buf, 36 + data_size);
    buf.insert(buf.end(), { 'W','A','V','E' });

    buf.insert(buf.end(), { 'f','m','t',' ' });
    write_le_32(buf, 16);
    write_le_16(buf, 1);
    write_le_16(buf, 1);
    write_le_32(buf, FREQ);
    write_le_32(buf, FREQ * sizeof(i16));
    write_le_16(buf, sizeof(i16));
    write_le_16(buf, 16);

    buf.insert(buf.end(), { 'd','a','t','a' });
    write_le_32(buf, data_size);
}

float midiNoteToFreq(int note) {
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

struct NoteEvent {
    float start;
    float dur;
    float freq;
};

int main() {
    std::string midiFilePath;
    std::cout << "Enter MIDI file path: ";
    std::getline(std::cin, midiFilePath);

    smf::MidiFile midifile;
    if (!midifile.read(midiFilePath)) {
        std::cerr << "Failed to read MIDI file.\n";
        return 1;
    }
    midifile.doTimeAnalysis();
    midifile.linkNotePairs();

    std::vector<NoteEvent> notes;
    for (int track = 0; track < midifile.getTrackCount(); ++track) {
        for (int i = 0; i < midifile[track].size(); ++i) {
            auto& event = midifile[track][i];
            if (event.isNoteOn()) {
                auto noteOff = event.getLinkedEvent();
                if (!noteOff) continue;

                float startSec = event.seconds;
                float endSec = noteOff->seconds;
                int midiNote = event.getKeyNumber();
                notes.push_back({ startSec, endSec - startSec, midiNoteToFreq(midiNote) });
            }
        }
    }

    float totalDur = 0.0f;
    for (auto& n : notes) totalDur = max(totalDur, n.start + n.dur);
    u32 num_samples = static_cast<u32>(totalDur * FREQ);

    std::vector<char> buffer;
    write_wav_header(buffer, num_samples);

    for (u32 i = 0; i < num_samples; ++i) {
        float t = float(i) / FREQ;
        float y = 0.0f;

        for (auto& n : notes) {
            if (t >= n.start && t <= n.start + n.dur) {
                y += 0.3f * sinf(2.0f * 3.14159265f * n.freq * (t - n.start));
            }
        }

        if (y > 1.0f) y = 1.0f;
        if (y < -1.0f) y = -1.0f;

        i16 sample = static_cast<i16>(y * INT16_MAX);
        write_le_16(buffer, sample);
    }

    while (!GetAsyncKeyState(VK_HOME)) {
        PlaySoundA(buffer.data(), NULL, SND_MEMORY);
        std::cout << "Looping sound." << std::endl;
    }

    return 0;
}

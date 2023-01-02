/*
  ZynAddSubFX - a software synthesizer

  OSSaudiooutput.C - Audio output for Open Sound System
  Copyright (C) 2002-2005 Nasca Octavian Paul
  Author: Nasca Octavian Paul

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License
  as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License (version 2 or later) for more details.

  You should have received a copy of the GNU General Public License (version 2)
  along with this program; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

*/

#include "NulEngine.h"
#include "../globals.h"

#include <iostream>

using namespace std;
using Clock = std::chrono::steady_clock;

NulEngine::NulEngine()
    :AudioOut(), pThread(NULL)
{
    name = "NULL";
    playing_until = Clock::time_point::min();
}

void *NulEngine::_AudioThread(void *arg)
{
    return (static_cast<NulEngine *>(arg))->AudioThread();
}

void *NulEngine::AudioThread()
{
    while(pThread) {
        getNext();

        using namespace std::literals::chrono_literals;
        const auto now = Clock::now();
        auto remaining = Clock::duration{};
        if(playing_until == Clock::time_point::min()) {
            playing_until = now;
        }
        else {
            remaining = playing_until - now;
            if(remaining > 10ms) //Don't sleep() less than 10ms.
                //This will add latency...
                std::this_thread::sleep_for(remaining - 10ms);
            if(remaining < 0ms)
                cerr << "WARNING - too late" << endl;
        }
        playing_until += synth->buffersize * std::chrono::duration_cast<Clock::duration>(1s)
                                 / synth->samplerate;
        if(remaining < 0ms)
            playing_until -= remaining;
    }
    return NULL;
}

NulEngine::~NulEngine()
{}

bool NulEngine::Start()
{
    setAudioEn(true);
    return getAudioEn();
}

void NulEngine::Stop()
{
    setAudioEn(false);
}

void NulEngine::setAudioEn(bool nval)
{
    if(nval) {
        if(!getAudioEn()) {
            pThread = new std::thread{_AudioThread, this};
        }
    }
    else
    if(getAudioEn()) {
        std::thread *thread = pThread;
        pThread = NULL;
        thread->join();
        delete thread;
    }
}

bool NulEngine::getAudioEn() const
{
    return pThread;
}

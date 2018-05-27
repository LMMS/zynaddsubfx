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

NulEngine::NulEngine()
    :AudioOut(), audioEnable(false)
{
    name = "NULL";
}


void NulEngine::AudioThread()
{
    //TODO: shouldn't playing_until be reset somewhere?
    while(audioEnable) {
        getNext();
        time_point now = clock::now();
        duration remaining;
        std::chrono::milliseconds latency(10);
        if(playing_until == time_point()) {
            playing_until = now;
        }
        else {
            remaining = playing_until - now;
            if(remaining > latency) //Don't sleep() less than 10ms.
                //This will add latency...
                std::this_thread::sleep_for(remaining - latency);
            if(remaining < duration::zero())
                cerr << "WARNING - too late" << endl;
        }
        playing_until += std::chrono::microseconds(synth->buffersize * 1000000 / synth->samplerate);
        if(remaining < duration::zero())
            playing_until -= remaining;
    }
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
            thread = std::thread(&NulEngine::AudioThread, this);
        }
    }
    else
    if(getAudioEn()) {
        audioEnable = false;
        thread.join();
    }
}

bool NulEngine::getAudioEn() const
{
    return audioEnable;
}

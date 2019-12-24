/*
  ZynAddSubFX - a software synthesizer

  ZynEffectMgr.cpp - ZynEffect manager, an interface betwen the program and effects
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

#include "ZynEffectMgr.h"
#include "ZynEffect.h"
#include "Reverb.h"
#include "Echo.h"
#include "Chorus.h"
#include "Distorsion.h"
#include "EQ.h"
#include "DynamicFilter.h"
#include "../Misc/XMLwrapper.h"
#include "../Params/FilterParams.h"

#include <iostream>
using namespace std;

ZynEffectMgr::ZynEffectMgr(const bool insertion_, pthread_mutex_t *mutex_)
    :insertion(insertion_),
      efxoutl(new float[synth->buffersize]),
      efxoutr(new float[synth->buffersize]),
      filterpars(NULL),
      nefx(0),
      efx(NULL),
      mutex(mutex_),
      dryonly(false)
{
    setpresettype("Peffect");
    memset(efxoutl, 0, synth->bufferbytes);
    memset(efxoutr, 0, synth->bufferbytes);
    defaults();
}


ZynEffectMgr::~ZynEffectMgr()
{
    delete efx;
    delete [] efxoutl;
    delete [] efxoutr;
}

void ZynEffectMgr::defaults(void)
{
    changeeffect(0);
    setdryonly(false);
}

//Change the effect
void ZynEffectMgr::changeeffect(int _nefx)
{
    cleanup();
    if(nefx == _nefx)
        return;
    nefx = _nefx;
    memset(efxoutl, 0, synth->bufferbytes);
    memset(efxoutr, 0, synth->bufferbytes);
    delete efx;
    switch(nefx) {
        case 1:
            efx = new Reverb(insertion, efxoutl, efxoutr, synth->samplerate, synth->buffersize);
            break;
        case 2:
            efx = new Echo(insertion, efxoutl, efxoutr, synth->samplerate, synth->buffersize);
            break;
        case 3:
            efx = new Chorus(insertion, efxoutl, efxoutr, synth->samplerate, synth->buffersize);
            break;
        case 4:
            efx = new Phaser(insertion, efxoutl, efxoutr, synth->samplerate, synth->buffersize);
            break;
        case 5:
            efx = new Alienwah(insertion, efxoutl, efxoutr, synth->samplerate, synth->buffersize);
            break;
        case 6:
            efx = new Distorsion(insertion, efxoutl, efxoutr, synth->samplerate, synth->buffersize);
            break;
        case 7:
            efx = new EQ(insertion, efxoutl, efxoutr, synth->samplerate, synth->buffersize);
            break;
        case 8:
            efx = new DynamicFilter(insertion, efxoutl, efxoutr, synth->samplerate, synth->buffersize);
            break;
        //put more effect here
        default:
            efx = NULL;
            break; //no effect (thru)
    }

    if(efx)
        filterpars = efx->filterpars;
}

//Obtain the effect number
int ZynEffectMgr::geteffect(void)
{
    return nefx;
}

// Cleanup the current effect
void ZynEffectMgr::cleanup(void)
{
    if(efx)
        efx->cleanup();
}


// Get the preset of the current effect
unsigned char ZynEffectMgr::getpreset(void)
{
    if(efx)
        return efx->Ppreset;
    else
        return 0;
}

// Change the preset of the current effect
void ZynEffectMgr::changepreset_nolock(unsigned char npreset)
{
    if(efx)
        efx->setpreset(npreset);
}

//Change the preset of the current effect(with thread locking)
void ZynEffectMgr::changepreset(unsigned char npreset)
{
    pthread_mutex_lock(mutex);
    changepreset_nolock(npreset);
    pthread_mutex_unlock(mutex);
}


//Change a parameter of the current effect
void ZynEffectMgr::seteffectpar_nolock(int npar, unsigned char value)
{
    if(!efx)
        return;
    efx->changepar(npar, value);
}

// Change a parameter of the current effect (with thread locking)
void ZynEffectMgr::seteffectpar(int npar, unsigned char value)
{
    pthread_mutex_lock(mutex);
    seteffectpar_nolock(npar, value);
    pthread_mutex_unlock(mutex);
}

//Get a parameter of the current effect
unsigned char ZynEffectMgr::geteffectpar(int npar)
{
    if(!efx)
        return 0;
    return efx->getpar(npar);
}

// Apply the effect
void ZynEffectMgr::out(float *smpsl, float *smpsr)
{
    if(!efx) {
        if(!insertion)
            for(int i = 0; i < synth->buffersize; ++i) {
                smpsl[i]   = 0.0f;
                smpsr[i]   = 0.0f;
                efxoutl[i] = 0.0f;
                efxoutr[i] = 0.0f;
            }
        return;
    }
    for(int i = 0; i < synth->buffersize; ++i) {
        smpsl[i]  += denormalkillbuf[i];
        smpsr[i]  += denormalkillbuf[i];
        efxoutl[i] = 0.0f;
        efxoutr[i] = 0.0f;
    }
    efx->out(smpsl, smpsr);

    float volume = efx->volume;

    if(nefx == 7) { //this is need only for the EQ effect
        memcpy(smpsl, efxoutl, synth->bufferbytes);
        memcpy(smpsr, efxoutr, synth->bufferbytes);
        return;
    }

    //Insertion effect
    if(insertion != 0) {
        float v1, v2;
        if(volume < 0.5f) {
            v1 = 1.0f;
            v2 = volume * 2.0f;
        }
        else {
            v1 = (1.0f - volume) * 2.0f;
            v2 = 1.0f;
        }
        if((nefx == 1) || (nefx == 2))
            v2 *= v2;  //for Reverb and Echo, the wet function is not liniar

        if(dryonly)   //this is used for instrument effect only
            for(int i = 0; i < synth->buffersize; ++i) {
                smpsl[i]   *= v1;
                smpsr[i]   *= v1;
                efxoutl[i] *= v2;
                efxoutr[i] *= v2;
            }
        else // normal instrument/insertion effect
            for(int i = 0; i < synth->buffersize; ++i) {
                smpsl[i] = smpsl[i] * v1 + efxoutl[i] * v2;
                smpsr[i] = smpsr[i] * v1 + efxoutr[i] * v2;
            }
    }
    else // System effect
        for(int i = 0; i < synth->buffersize; ++i) {
            efxoutl[i] *= 2.0f * volume;
            efxoutr[i] *= 2.0f * volume;
            smpsl[i]    = efxoutl[i];
            smpsr[i]    = efxoutr[i];
        }
}


// Get the effect volume for the system effect
float ZynEffectMgr::sysefxgetvolume(void)
{
    return (!efx) ? 1.0f : efx->outvolume;
}


// Get the EQ response
float ZynEffectMgr::getEQfreqresponse(float freq)
{
    return (nefx == 7) ? efx->getfreqresponse(freq) : 0.0f;
}


void ZynEffectMgr::setdryonly(bool value)
{
    dryonly = value;
}

void ZynEffectMgr::add2XML(XMLwrapper *xml)
{
    xml->addpar("type", geteffect());

    if(!efx || !geteffect())
        return;
    xml->addpar("preset", efx->Ppreset);

    xml->beginbranch("EFFECT_PARAMETERS");
    for(int n = 0; n < 128; ++n) {
        int par = geteffectpar(n);
        if(par == 0)
            continue;
        xml->beginbranch("par_no", n);
        xml->addpar("par", par);
        xml->endbranch();
    }
    if(filterpars) {
        xml->beginbranch("FILTER");
        filterpars->add2XML(xml);
        xml->endbranch();
    }
    xml->endbranch();
}

void ZynEffectMgr::getfromXML(XMLwrapper *xml)
{
    changeeffect(xml->getpar127("type", geteffect()));

    if(!efx || !geteffect())
        return;

    efx->Ppreset = xml->getpar127("preset", efx->Ppreset);

    if(xml->enterbranch("EFFECT_PARAMETERS")) {
        for(int n = 0; n < 128; ++n) {
            seteffectpar_nolock(n, 0); //erase effect parameter
            if(xml->enterbranch("par_no", n) == 0)
                continue;
            int par = geteffectpar(n);
            seteffectpar_nolock(n, xml->getpar127("par", par));
            xml->exitbranch();
        }
        if(filterpars)
            if(xml->enterbranch("FILTER")) {
                filterpars->getfromXML(xml);
                xml->exitbranch();
            }
        xml->exitbranch();
    }
    cleanup();
}

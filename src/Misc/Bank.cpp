/*
  ZynAddSubFX - a software synthesizer

  Bank.cpp - Instrument Bank
  Copyright (C) 2002-2005 Nasca Octavian Paul
  Copyright (C) 2010-2010 Mark McCurry
  Author: Nasca Octavian Paul
          Mark McCurry

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

#include "Bank.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <algorithm>
#include <iostream>

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#ifdef WIN32
#include <Windows.h>
#endif

#include "Config.h"
#include "Util.h"
#include "Part.h"

#define INSTRUMENT_EXTENSION ".xiz"

//if this file exists into a directory, this make the directory to be considered as a bank, even if it not contains a instrument file
#define FORCE_BANK_DIR_FILE ".bankdir"

using namespace std;

Bank::Bank()
    :defaultinsname(" ")
{
    clearbank();
    bankfiletitle = dirname;
    loadbank(config.cfg.currentBankDir);
}

Bank::~Bank()
{
    clearbank();
}

/*
 * Get the name of an instrument from the bank
 */
string Bank::getname(unsigned int ninstrument)
{
    if(emptyslot(ninstrument))
        return defaultinsname;
    return ins[ninstrument].name;
}

/*
 * Get the numbered name of an instrument from the bank
 */
string Bank::getnamenumbered(unsigned int ninstrument)
{
    if(emptyslot(ninstrument))
        return defaultinsname;

    return stringFrom(ninstrument + 1) + ". " + getname(ninstrument);
}

/*
 * Changes the name of an instrument (and the filename)
 */
void Bank::setname(unsigned int ninstrument, const string &newname, int newslot)
{
    if(emptyslot(ninstrument))
        return;

    string newfilename;
    char   tmpfilename[100 + 1];
    tmpfilename[100] = 0;

    if(newslot >= 0)
        snprintf(tmpfilename, 100, "%4d-%s", newslot + 1, newname.c_str());
    else
        snprintf(tmpfilename, 100, "%4d-%s", ninstrument + 1, newname.c_str());

    //add the zeroes at the start of filename
    for(int i = 0; i < 4; ++i)
        if(tmpfilename[i] == ' ')
            tmpfilename[i] = '0';

    newfilename = dirname + '/' + legalizeFilename(tmpfilename) + ".xiz";

    rename(ins[ninstrument].filename.c_str(), newfilename.c_str());

    ins[ninstrument].filename = newfilename;
    ins[ninstrument].name     = newname;
}

/*
 * Check if there is no instrument on a slot from the bank
 */
bool Bank::emptyslot(unsigned int ninstrument)
{
    if(ninstrument >= BANK_SIZE)
        return true;
    if(ins[ninstrument].filename.empty())
        return true;

    if(ins[ninstrument].used)
        return false;
    else
        return true;
}

/*
 * Removes the instrument from the bank
 */
void Bank::clearslot(unsigned int ninstrument)
{
    if(emptyslot(ninstrument))
        return;

    remove(ins[ninstrument].filename.c_str());
    deletefrombank(ninstrument);
}

/*
 * Save the instrument to a slot
 */
void Bank::savetoslot(unsigned int ninstrument, Part *part)
{
    clearslot(ninstrument);

    const int maxfilename = 200;
    char      tmpfilename[maxfilename + 20];
    ZERO(tmpfilename, maxfilename + 20);

    snprintf(tmpfilename,
             maxfilename,
             "%4d-%s",
             ninstrument + 1,
             (char *)part->Pname);

    //add the zeroes at the start of filename
    for(int i = 0; i < 4; ++i)
        if(tmpfilename[i] == ' ')
            tmpfilename[i] = '0';

    string filename = dirname + '/' + legalizeFilename(tmpfilename) + ".xiz";

    remove(filename.c_str());
    part->saveXML(filename.c_str());
    addtobank(ninstrument, legalizeFilename(tmpfilename) + ".xiz", (char *) part->Pname);
}

/*
 * Loads the instrument from the bank
 */
void Bank::loadfromslot(unsigned int ninstrument, Part *part)
{
    if(emptyslot(ninstrument))
        return;

    part->AllNotesOff();
    part->defaultsinstrument();

    part->loadXMLinstrument(ins[ninstrument].filename.c_str());
}

/*
 * Makes current a bank directory
 */
int Bank::loadbank(string bankdirname)
{
    std::vector<std::string> files = GetDirectory(bankdirname.c_str());
    clearbank();

    if(files.empty())
        return -1;

    dirname = bankdirname;

    bankfiletitle = dirname;


    for(const std::string& file : files) {

        //check for extension
        if(file.find(INSTRUMENT_EXTENSION) == std::string::npos)
            continue;

        //verify if the name is like this NNNN-name (where N is a digit)
        int no = 0;
        unsigned int startname = 0;

        //TODO: maybe replace with std::regex?
        for(unsigned int i = 0; i < 4; ++i) {
            if(file.length() < i)
                break;

            if((file[i] >= '0') && (file[i] <= '9')) {
                no = no * 10 + (file[i] - '0');
                startname++;
            }
        }

        if((startname + 1) < file.length())
            startname++;  //to take out the "-"

        string name = file;

        //remove the file extension
        for(int i = name.size() - 1; i >= 2; i--)
            if(name[i] == '.') {
                name = name.substr(0, i);
                break;
            }

        if(no != 0) //the instrument position in the bank is found
            addtobank(no - 1, file, name.substr(startname));
        else
            addtobank(-1, file, name);
    }

    if(!dirname.empty())
        config.cfg.currentBankDir = dirname;

    return 0;
}

/*
 * Makes a new bank, put it on a file and makes it current bank
 */
int Bank::newbank(string newbankdirname)
{
    string bankdir;
    bankdir = config.cfg.bankRootDirList[0];

    if(((bankdir[bankdir.size() - 1]) != '/')
       && ((bankdir[bankdir.size() - 1]) != '\\'))
        bankdir += "/";

    bankdir += newbankdirname;
#ifdef WIN32
    if(!CreateDirectory(bankdir.c_str(), nullptr))
#else
    if(mkdir(bankdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0)
#endif
        return -1;

    const string tmpfilename = bankdir + '/' + FORCE_BANK_DIR_FILE;

    FILE *tmpfile = fopen(tmpfilename.c_str(), "w+");
    fclose(tmpfile);

    return loadbank(bankdir);
}

/*
 * Check if the bank is locked (i.e. the file opened was readonly)
 */
int Bank::locked()
{
    return dirname.empty();
}

/*
 * Swaps a slot with another
 */
void Bank::swapslot(unsigned int n1, unsigned int n2)
{
    if((n1 == n2) || (locked()))
        return;
    if(emptyslot(n1) && (emptyslot(n2)))
        return;
    if(emptyslot(n1)) //change n1 to n2 in order to make
        swap(n1, n2);

    if(emptyslot(n2)) { //this is just a movement from slot1 to slot2
        setname(n1, getname(n1), n2);
        ins[n2] = ins[n1];
        ins[n1] = ins_t();
    }
    else {  //if both slots are used
        if(ins[n1].name == ins[n2].name) //change the name of the second instrument if the name are equal
            ins[n2].name += "2";

        setname(n1, getname(n1), n2);
        setname(n2, getname(n2), n1);
        swap(ins[n2], ins[n1]);
    }
}


bool Bank::bankstruct::operator<(const bankstruct &b) const
{
    return name < b.name;
}

/*
 * Re-scan for directories containing instrument banks
 */

void Bank::rescanforbanks()
{
    //remove old banks
    banks.clear();

    for(int i = 0; i < MAX_BANK_ROOT_DIRS; ++i)
        if(!config.cfg.bankRootDirList[i].empty())
            scanrootdir(config.cfg.bankRootDirList[i]);

    //sort the banks
    sort(banks.begin(), banks.end());

    //remove duplicate bank names
    int dupl = 0;
    for(int j = 0; j < (int) banks.size() - 1; ++j)
        for(int i = j + 1; i < (int) banks.size(); ++i) {
            if(banks[i].name == banks[j].name) {
                //add a [1] to the first bankname and [n] to others
                banks[i].name = banks[i].name + '['
                                + stringFrom(dupl + 2) + ']';
                if(dupl == 0)
                    banks[j].name += "[1]";

                dupl++;
            }
            else
                dupl = 0;
        }
}


// private stuff

void Bank::scanrootdir(string rootdir)
{
    std::vector<std::string> files = GetDirectory(rootdir.c_str());
    if(files.empty())
        return;

    bankstruct bank;

    const char *separator = "/";
    if(rootdir.size()) {
        char tmp = rootdir[rootdir.size() - 1];
        if((tmp == '/') || (tmp == '\\'))
            separator = "";
    }

    for (const std::string& file : files)
    {
        if (file[0] == '.')
            continue;

        bank.dir = rootdir + separator + file + '/';
        bank.name = file;

        //find out if the directory contains at least 1 instrument
        bool isbank = false;
        std::vector<std::string> bankFiles = GetDirectory(bank.dir.c_str());

        if (bankFiles.empty())
            continue;

        for (const std::string& bankFile : bankFiles)
        {
            if ((bankFile.find(INSTRUMENT_EXTENSION) != std::string::npos)
                || (bankFile.find(FORCE_BANK_DIR_FILE) != std::string::npos)) {
                isbank = true;
                break;
            }
        }

        if(isbank)
            banks.push_back(bank);
    }
}

void Bank::clearbank()
{
    for(int i = 0; i < BANK_SIZE; ++i)
        ins[i] = ins_t();

    bankfiletitle.clear();
    dirname.clear();
}

int Bank::addtobank(int pos, string filename, string name)
{
    if((pos >= 0) && (pos < BANK_SIZE)) {
        if(ins[pos].used)
            pos = -1;  //force it to find a new free position
    }
    else
    if(pos >= BANK_SIZE)
        pos = -1;


    if(pos < 0)   //find a free position
        for(int i = BANK_SIZE - 1; i >= 0; i--)
            if(!ins[i].used) {
                pos = i;
                break;
            }

    if(pos < 0)
        return -1;  //the bank is full

    deletefrombank(pos);

    ins[pos].used     = true;
    ins[pos].name     = name;
    ins[pos].filename = dirname + '/' + filename;

    //see if PADsynth is used
    if(config.cfg.CheckPADsynth) {
        XMLwrapper xml;
        xml.loadXMLfile(ins[pos].filename);

        ins[pos].info.PADsynth_used = xml.hasPadSynth();
    }
    else
        ins[pos].info.PADsynth_used = false;

    return 0;
}

bool Bank::isPADsynth_used(unsigned int ninstrument)
{
    if(config.cfg.CheckPADsynth == 0)
        return 0;
    else
        return ins[ninstrument].info.PADsynth_used;
}


void Bank::deletefrombank(int pos)
{
    if((pos < 0) || (pos >= BANK_SIZE))
        return;
    ins[pos] = ins_t();
}

Bank::ins_t::ins_t()
    :used(false), name(""), filename("")
{
    info.PADsynth_used = false;
}

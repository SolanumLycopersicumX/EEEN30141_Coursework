//  cs_assignment_file.hpp
// Created by mchssfp3 on 29/10/2024.
#ifndef cs_assignment_file_hpp
#define cs_assignment_file_hpp
#include <stdio.h>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
using namespace std;

class Team{
private:
    string teamName;
    float fTeamTime_s=0;
    int iNumBatonExchanges = 0;
    std::mutex mtx;   // Mutex is non-movable prior to C++17, so that you cannot use them in a C[] type array
public:
    Team() {;}
    Team(string tN): teamName(tN) {}
    void setTeam(string tN){teamName=tN; fTeamTime_s=0;}
    string getTeam(){return teamName;}
    void addTime(float fCompetitorTime_s) // thread-safe
    {
        std::lock_guard<std::mutex> lock(mtx);
        fTeamTime_s += fCompetitorTime_s;
        iNumBatonExchanges++;
    }
    int numBatonExchanges() {return iNumBatonExchanges;}
    float getTime() { return fTeamTime_s; }
    void printTimes() {
        std::cout << "Team " << teamName << " = " << fTeamTime_s << " s"<< std::endl;
    }
};

class Competitor { // created in main and never updated, passed to a thread, placed in map
private:
    string personName;
    Team *pTeam=NULL; // Can be global
    float _fTime_s=0;
public:
    std::condition_variable baton; // Shared with the following athlete's thread
    std::mutex mtx; // Shared with the following athlete's thread
    bool bFinished = false;
    Competitor(){;}
    Competitor(string pN, Team *pT) : personName(pN), pTeam(pT){}
    void set(string pN, Team *pT) { personName=pN; pTeam=pT; }
    void setTime(float fT_s){
        _fTime_s=fT_s;
        pTeam->addTime(fT_s);
        bFinished = true;
        std::lock_guard<std::mutex> lock(mtx);
        baton.notify_one(); // Hand over the baton
    }
    float getTime(){return _fTime_s;}
    string getTeamName() { return pTeam->getTeam(); }
    void setPerson(string pN){ personName = pN; }
    string getPerson(){ return personName; }
    int numBatonExchanges() {return pTeam->numBatonExchanges();}
    void printCompetitor(){
        std::cout << "Competitor: Team= " << getTeamName() << ", Person = " << personName << ", Time = " << _fTime_s << " s" << std::endl;
    }
};
#endif /* cs_assignment_file_hpp */

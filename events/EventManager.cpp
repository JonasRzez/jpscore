/**
 * \file        EventManager.cpp
 * \date        Jul 4, 2014
 * \version     v0.7
 * \copyright   <2009-2015> Forschungszentrum Jülich GmbH. All rights reserved.
 *
 * \section License
 * This file is part of JuPedSim.
 *
 * JuPedSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * JuPedSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with JuPedSim. If not, see <http://www.gnu.org/licenses/>.
 *
 * \section Description
 *
 *
 **/


#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <vector>
#include <math.h>
#include <stdio.h>
#include "../pedestrian/Pedestrian.h"
#include "../pedestrian/Knowledge.h"
#include "../mpi/LCGrid.h"
#include "../geometry/Building.h"
#include "../geometry/SubRoom.h"
#include "../geometry/Transition.h"
#include "../geometry/Point.h"
#include "../tinyxml/tinyxml.h"
#include "../IO/OutputHandler.h"
#include "../IO/IODispatcher.h"
#include "../routing/RoutingEngine.h"
#include "../routing/GlobalRouter.h"
#include "../routing/QuickestPathRouter.h"
#include "../routing/MeshRouter.h"
#include "../routing/DummyRouter.h"
#include "../routing/SafestPathRouter.h"
#include "../routing/CognitiveMapRouter.h"
#include "EventManager.h"
#include "Event.h"

using std::map;
using std::cout;
using std::endl;


EventManager::EventManager(Building *_b)
{
     _building = _b;
     _eventCounter = 0;
     _dynamic = false;
     _lastUpdateTime = 0;
     _projectFilename=_building->GetProjectFilename();
     _projectRootDir=_building->GetProjectRootDir();
     _updateFrequency =1 ;//seconds
     _updateRadius =2;//meters

     _file = fopen("../events/events.txt", "r");
     if (!_file) {
          Log->Write("INFO:\tFiles 'events.txt' missing. "
                    "Realtime interaction with the simulation not possible.");
     } else {
          Log->Write("INFO:\tFile 'events.txt' will be monitored for new events.");
          _dynamic = true;
     }

     //generate random number between 0 and 1 uniformly distributed
     _rdDistribution = std::uniform_real_distribution<double> (0,1);
     //std::random_device rd;
     //_rdGenerator=std::mt19937(rd());
     _rdGenerator=std::mt19937(23);


     //save the first graph
     CreateRoutingEngine(_b, true);

     //create and
     CreateSomeEngine();
}

EventManager::~EventManager()
{
     if(_file)
          fclose(_file);

     _eventEngineStorage.clear();
}

bool EventManager::ReadEventsXml()
{
     Log->Write("INFO: \tLooking for pre-defined events in other files");
     //get the geometry filename from the project file
     TiXmlDocument doc(_projectFilename);
     if (!doc.LoadFile()) {
          Log->Write("ERROR: \t%s", doc.ErrorDesc());
          Log->Write("ERROR: \t could not parse the project file.");
          return false;
     }

     TiXmlElement* xMainNode = doc.RootElement();
     string eventfile = "";
     if (xMainNode->FirstChild("events_file")) {
          eventfile = _projectRootDir
                    + xMainNode->FirstChild("events_file")->FirstChild()->Value();
          Log->Write("INFO: \tevents <" + eventfile + ">");
     } else {
          Log->Write("INFO: \tNo events found");
          return true;
     }

     Log->Write("INFO: \tParsing the event file");
     TiXmlDocument docEvent(eventfile);
     if (!docEvent.LoadFile()) {
          Log->Write("ERROR: \t%s", docEvent.ErrorDesc());
          Log->Write("ERROR: \t could not parse the event file.");
          return false;
     }

     TiXmlElement* xRootNode = docEvent.RootElement();
     if (!xRootNode) {
          Log->Write("ERROR:\tRoot element does not exist.");
          return false;
     }

     if (xRootNode->ValueStr() != "JPScore") {
          Log->Write("ERROR:\tRoot element value is not 'JPScore'.");
          return false;
     }

     TiXmlNode* xEvents = xRootNode->FirstChild("events");
     if (!xEvents) {
          Log->Write("ERROR:\tNo events found.");
          return false;
     }
     _updateFrequency = xmltoi(xEvents->ToElement()->Attribute("update_frequency"), 1);
     _updateRadius = xmltoi(xEvents->ToElement()->Attribute("update_radius"), 2);

     //Pedestrian::SetColorMode(BY_SPOTLIGHT);

     for (TiXmlElement* e = xEvents->FirstChildElement("event"); e;
               e = e->NextSiblingElement("event")) {

          int id = atoi(e->Attribute("id"));
          double zeit = atoi(e->Attribute("time"));
          string state (e->Attribute("state"));
          string type (e->Attribute("type"));
          _events.push_back(Event(id,zeit,type,state));
     }
     Log->Write("INFO: \tEvents were initialized");
     return true;
}

void EventManager::ListEvents()
{
     for(const auto& event: _events)
     {
          Log->Write("INFO:\t " + event.GetDescription());
     }
}

void EventManager::ReadEventsTxt(double time)
{
     rewind(_file);
     char cstring[256];
     int lines = 0;
     do {
          if (fgets(cstring, 30, _file) == NULL) {
               Log->Write("WARNING: \tCould not read the event file");
               return;
          }
          if (cstring[0] != '#') {// skip comments
               lines++;
               if (lines > _eventCounter) {
                    Log->Write("INFO:\tEvent: after %.2f sec: ", time);
                    GetEvent(cstring);
                    _eventCounter++;
               }
          }
     } while (feof(_file) == 0);
}

bool EventManager::UpdateAgentKnowledge(Building* _b)
{
     //#pragma omp parallel
     for(auto&& ped:_b->GetAllPedestrians())
     {
          for (auto&& door: _b->GetAllTransitions())
          {
               if(door.second->DistTo(ped->GetPos())<0.5)//distance to door to register its state
               {
                    //actualize the information about the newly closed door
                    if(door.second->IsOpen()==false)
                    {
                         //1.0 because the information is sure
                         ped->AddKnownClosedDoor(door.first, Pedestrian::GetGlobalTime(), !door.second->IsOpen(),_updateFrequency,1.0);
                         ped->SetNewEventFlag(true);
                    }
               }
          }
     }

     //collect the peds that are allowed to forward the information.
     vector<Pedestrian*> informant;
     for(auto&& ped:_b->GetAllPedestrians())
     {
          //if (ped->GetNewEventFlag())
               informant.push_back(ped);


          for(auto&& info: ped->GetKnownledge())
          {
               info.second.DecreaseLatency(_updateFrequency);
          }
     }


     for(auto&& ped1:informant)
     {
          vector<Pedestrian*> neighbourhood;
          _b->GetGrid()->GetNeighbourhood(ped1,neighbourhood);
          for(auto&& ped2:neighbourhood)
          {
               if( (ped1->GetPos()-ped2->GetPos()).Norm()<_updateRadius)
               {
                    //maybe same room and subroom ?
                    vector<SubRoom*> empty;
                    if(_b->IsVisible(ped1->GetPos(),ped2->GetPos(),empty))
                    {
                         //if(SynchronizeKnowledge(ped1, ped2))  //ped1->SetSpotlight(true);
                         //if(MergeKnowledgeUsingProbability(ped1, ped2))
                         if(MergeKnowledge(ped1, ped2))
                         {
                              //p2 is now an informant
                              ped2->SetNewEventFlag(true);
                         }
                    }
               }
          }
     }

     //TODO: what happen when they all have the new event flag ? reset maybe?
     if(informant.size()==_b->GetAllPedestrians().size())
     {

          //Log->Write("INFO:\t all pedestrians are now propagating information");
          for(auto&& ped:_b->GetAllPedestrians())
               ped->SetNewEventFlag(false);
     }

     // information speed to fast
     //     for(auto&& ped1:_b->GetAllPedestrians())
     //     {
     //          vector<Pedestrian*> neighbourhood;
     //          _b->GetGrid()->GetNeighbourhood(ped1,neighbourhood);
     //          for(auto&& ped2:neighbourhood)
     //          {
     //               if( (ped1->GetPos()-ped2->GetPos()).Norm()<_updateRadius)
     //               {
     //                    //maybe same room and subroom ?
     //                    if(_b->IsVisible(ped1->GetPos(),ped2->GetPos()))
     //                    MergeKnowledge(ped1, ped2);  //ped1->SetSpotlight(true);
     //               }
     //          }
     //     }

     //update the routers based on the configurations
     //#pragma omp parallel
     for(auto&& ped:_b->GetAllPedestrians())
     {
          if(UpdateRoute(ped)==false)
          {
               //Clear the memory and attempt to reroute
               //this can happen if all doors are known to be closed
               ped->ClearKnowledge();
               Log->Write("ERROR: \t clearing ped knowledge");
               //ped->Dump(ped->GetID());
               if(UpdateRoute(ped)==false)
               {
                    Log->Write("ERROR: \t cannot reroute the pedestrian. unknown problem");
                    //return false;
                    exit(EXIT_FAILURE);
               }
          }
     }
     return true;
}

bool EventManager::UpdateRoute(Pedestrian* ped)
{
     //create the key as string.
     //map are sorted by default
     string key= ped->GetKnowledgeAsString();
     //get the router engine corresponding to the actual configuration
     bool status=true;
     if (_eventEngineStorage.count(key)>0)
     {
          RoutingEngine* engine=_eventEngineStorage[key];
          //retrieve the old strategy
          RoutingStrategy strategy=ped->GetRouter()->GetStrategy();
          //retrieve the new router
          Router*rout =engine->GetRouter(strategy);
          //check for validity
          ped->SetRouter(rout);
          //clear all previous routes
          ped->ClearMentalMap();
          //ped->ClearKnowledge();
          //overwrite/update the pedestrian router
          if(!rout) status= false;
     }
     else
     {
          //Log->Write("WARNING: \t unknown configuration <%s>", key.c_str());
          //Log->Write("WARNING: \t  [%d] router available", _eventEngineStorage.size());
          //Log->Write("       : \t trying to create");
          //CreateRoutingEngine(_building);
          status= false;
     }
     return status;
}

bool EventManager::SynchronizeKnowledge(Pedestrian* p1, Pedestrian* p2)
{
     auto const & old_info1 = p1->GetKnownledge();
     auto const & old_info2 = p2->GetKnownledge();
     map<int, Knowledge> merge_info;

     //collect the most recent knowledge
     for (auto&& info1 : old_info1)
     {
          merge_info[info1.first] = info1.second;
     }

     for (auto&& info2 : old_info2)
     {
          //update infos according to a newest time
          if (merge_info.count(info2.first) > 0)
          {
               if (info2.second.GetTime() > merge_info[info2.first].GetTime())
               {
                    merge_info[info2.first] = info2.second;
               }
          }
          else //the info was not present, just add
          {
               merge_info[info2.first] = info2.second;
          }
     }

     //synchronize the knowledge
     p1->ClearKnowledge();
     p2->ClearKnowledge();
     for (auto&& info : merge_info)
     {
          p1->AddKnownClosedDoor(info.first, info.second.GetTime(),
                    info.second.GetState(), info.second.GetQuality(),_updateFrequency);
          p2->AddKnownClosedDoor(info.first, info.second.GetTime(),
                    info.second.GetState(), info.second.GetQuality(),_updateFrequency);
     }
     return true;
}

bool EventManager::MergeKnowledgeUsingProbability(Pedestrian* p1, Pedestrian* p2)
{
     auto const & old_info1 = p1->GetKnownledge();
     auto const & old_info2 = p2->GetKnownledge();

     map<int, Knowledge> merge_info;
     //collect the most recent knowledge
     //only the knowledge that has been accepted
     for (auto&& info1 : old_info1)
     {
          merge_info[info1.first] = info1.second;
     }

     for (auto&& info2 : old_info2)
     {
          //update infos according to a newest time
          if (merge_info.count(info2.first) > 0)
          {
               if (info2.second.GetTime() > merge_info[info2.first].GetTime())
                    // and the quality
                    // only if I never refused that information
               {
                    merge_info[info2.first] = info2.second;
               }
          }
          else //the info was not present, just add
          {
               merge_info[info2.first] = info2.second;
          }
     }

     //synchronize the knowledge
     //accept the information with a certain probability
     if(_rdDistribution(_rdGenerator)< (1-p1->GetRiskTolerance()))
     {
          //p1->ClearKnowledge();
          p2->ClearKnowledge();
          for (auto&& info : merge_info)
          {
               p2->AddKnownClosedDoor(info.first, info.second.GetTime(),info.second.GetState(),_updateFrequency,1.0);
          }
          //p2->SetSpotlight(false);
          return true;
     }
     else
     {
          cout<<"refusing the information:"<<p2->GetID()<<endl;
          //Pedestrian::SetColorMode(BY_SPOTLIGHT);
          //p2->SetSpotlight(true);
          //exit(0);
          return false;
     }


}

bool EventManager::MergeKnowledge(Pedestrian* p1, Pedestrian* p2)
{
     auto const & old_info1 = p1->GetKnownledge();
     auto & old_info2 = p2->GetKnownledge();
     //accept the new information
     if(_rdDistribution(_rdGenerator)< (1-p1->GetRiskTolerance()))
     {
          for (const auto& info1 : old_info1)
          {
               //I dont forward information that I refused already
               if(info1.second.HasBeenRefused()) continue;

               // Is the latency ok ?
               if(!info1.second.CanBeForwarded()) continue;

               //do I already have that information ?
               if (old_info2.count(info1.first) > 0)
               {
                    //only accept if it is newer
                    if (info1.second.GetTime() > old_info2[info1.first].GetTime())
                    {
                         //maybe I already refused that information earlier. Keep refusing
                         if(old_info2[info1.first].HasBeenRefused()==false)
                         {
                              old_info2[info1.first]=info1.second;
                              //alter the quality of the info
                              old_info2[info1.first].SetQuality(0.5);
                              old_info2[info1.first].SetLatency(_updateFrequency);
                         }
                    }
               }
               else
               {
                    //new piece of information
                    old_info2[info1.first]=info1.second;
                    //alter the quality of the info
                    old_info2[info1.first].SetQuality(0.5);
                    old_info2[info1.first].SetLatency(_updateFrequency);
               }
          }
          return true;
     }
     //refuse the new information
     else
     {
          for (const auto& info1 : old_info1)
          {
               if (old_info2.count(info1.first) > 0)
               {
                    old_info2[info1.first].Refuse(true);
                    old_info2[info1.first].SetLatency(_updateFrequency);
                    //cout<<"refusing present: "<<p2->GetID()<<endl;
               }
               else
               {//refuse the information and set a bad quality
                    old_info2[info1.first]=info1.second;
                    //alter the quality of the info
                    old_info2[info1.first].SetQuality(0.0);
                    old_info2[info1.first].Refuse(true);
                    old_info2[info1.first].SetLatency(_updateFrequency);
                    //cout<<"refusing: "<<p2->GetID()<<endl;
               }
          }
          //p2->SetSpotlight(true);
          //Pedestrian::SetColorMode(BY_SPOTLIGHT);
          cout<<"refusing..."<<p2->GetID()<<endl;
          return false;
     }

}

void EventManager::ProcessEvent()
{
     if (_events.size() == 0) return;

     int current_time = Pedestrian::GetGlobalTime();

     if ( (current_time != _lastUpdateTime) &&
               ((current_time % _updateFrequency) == 0))
     {
          //update knowledge about closed doors
          //share the information between the pedestrians
          UpdateAgentKnowledge(_building);
          //actualize based on the new knowledge
          _lastUpdateTime = current_time;
          //cout<<"update: "<<current_time<<endl;
     }

     //update the building state
     // the time is needed as double
     double current_time_d = Pedestrian::GetGlobalTime();

     for(const auto& event: _events)
     {
          if (fabs(event.GetTime() - current_time_d) < J_EPS_EVENT) {
               //Event with current time stamp detected
               Log->Write("INFO:\tEvent: after %.2f sec: ", current_time_d);
               if (event.GetState().compare("close") == 0) {
                    CloseDoor(event.GetId());
               } else {
                    OpenDoor(event.GetId());
               }
          }

     }

     if (_dynamic)
          ReadEventsTxt(current_time);
}



//close the door if it was open and relaunch the routing procedure
void EventManager::CloseDoor(int id)
{
     Transition *t = _building->GetTransition(id);
     if (t->IsOpen())
     {
          t->Close();
          Log->Write("INFO:\tClosing door %d ", id);
          //Create and save a graph corresponding to the actual state of the building.
          if(CreateRoutingEngine(_building)==false)
          {
               Log->Write("ERROR: \tcannot create a routing engine with the new event");
          }
     } else {
          Log->Write("WARNING: \tdoor %d is already close", id);
     }

}

//open the door if it was open and relaunch the routing procedure
void EventManager::OpenDoor(int id)
{
     Transition *t = _building->GetTransition(id);
     if (!t->IsOpen())
     {
          t->Open();
          Log->Write("INFO:\tOpening door %d ", id);
          //Create and save a graph corresponding to the actual state of the building.
          if(CreateRoutingEngine(_building)==false)
          {
               Log->Write("ERROR: \tcannot create a routing engine with the new event");
          }
     } else {
          Log->Write("WARNING: \tdoor %d is already open", id);
     }
}

void EventManager::GetEvent(char* c)
{
     int split = 0;
     string type = "";
     string id = "";
     string state = "";
     for (int i = 0; i < 20; i++) {
          if (!c[i]) {
               break;
          } else if (c[i] == ' ') {
               split++;
          } else if (c[i] == '\n') {

          } else {
               if (split == 0) {
                    type += c[i];
               } else if (split == 1) {
                    id += c[i];
               } else if (split == 2) {
                    state += c[i];
               }
          }
     }
     if (state.compare("close") == 0) {
          CloseDoor(atoi(id.c_str()));
     } else {
          OpenDoor(atoi(id.c_str()));
     }
}

bool EventManager::CreateRoutingEngine(Building* _b, int first_engine)
{
     std::vector<int> closed_doors;
     closed_doors.clear();

     for(auto&& t:_b->GetAllTransitions())
     {
          if(t.second->IsOpen()==false)
               closed_doors.push_back(t.second->GetID());
     }
     std::sort(closed_doors.begin(), closed_doors.end());

     //create the key as string.
     string key="";
     for(int door:closed_doors)
     {
          if(key.empty())
               key.append(std::to_string(door));
          else
               key.append(":"+std::to_string(door));
     }

     //the first (default) engine was created in the simulation
     // collect the defined routers
     if(first_engine)
     {
          RoutingEngine* engine=_b->GetRoutingEngine();
          _eventEngineStorage[key]=engine;

          for(auto&& rout: engine->GetAvailableRouters())
          {
               _availableRouters.push_back(rout->GetStrategy());
          }
          Log->Write("INFO: \tAdding a new routing Engine with the key: "+key+"\n");
          return true;
     }

     // the engine was not created
     // create a new one with the actual configuration
     if (_eventEngineStorage.count(key)==0)
     {

          //populate the engine with the routers defined in the ini file
          //and initialize
          RoutingEngine* engine= new RoutingEngine();
          for(auto&& rout:_availableRouters)
          {
               engine->AddRouter(CreateRouter(rout));
          }

          if(engine->Init(_b)==false)
               return false;

          //save the configuration
          _eventEngineStorage[key]=engine;
          Log->Write("INFO: \tAdding a new routing Engine with the key: "+key+"\n");
     }
     else
     {
          Log->Write("INFO: \tA routing already exits with the key: "+key+"\n");
     }

     return true;
}

Router * EventManager::CreateRouter(const RoutingStrategy& strategy)
{
     Router * rout=nullptr;

     switch(strategy)
     {
     case ROUTING_LOCAL_SHORTEST:
          rout = new GlobalRouter(ROUTING_LOCAL_SHORTEST, ROUTING_LOCAL_SHORTEST);
          break;

     case ROUTING_GLOBAL_SHORTEST:
          rout = new GlobalRouter(ROUTING_GLOBAL_SHORTEST, ROUTING_GLOBAL_SHORTEST);
          break;

     case ROUTING_QUICKEST:
          rout = new QuickestPathRouter(ROUTING_QUICKEST, ROUTING_QUICKEST);
          break;

     case ROUTING_NAV_MESH:
          rout = new MeshRouter(ROUTING_NAV_MESH, ROUTING_NAV_MESH);
          break;

     case ROUTING_DUMMY:
          rout = new DummyRouter(ROUTING_DUMMY, ROUTING_DUMMY);
          break;

     case ROUTING_SAFEST:
          rout = new SafestPathRouter(ROUTING_SAFEST, ROUTING_SAFEST);
          break;

     case ROUTING_COGNITIVEMAP:
          rout = new CognitiveMapRouter(ROUTING_COGNITIVEMAP, ROUTING_COGNITIVEMAP);
          break;

     default:
          Log->Write("ERROR: \twrong value for routing strategy [%d]!!!\n", strategy );
          exit(EXIT_FAILURE);
          break;

     }
     return rout;
}

void EventManager::CreateSomeEngine()
{
     Log->Write("INFO: \tpopulating routers");
     std::map<int, bool> doors_states;

     //save the doors states
     for(auto&& t:_building->GetAllTransitions())
     {
          doors_states[t.second->GetID()]=    t.second->IsOpen();
     }

     //open all doors
     for(auto&& t:_building->GetAllTransitions())
     {
          t.second->Open();
     }

     //close the doors one by one and create engines
     for(auto&& t1:_building->GetAllTransitions())
     {
          for(auto&& t2:_building->GetAllTransitions())
          {
               t2.second->Open();
          }
          t1.second->Close();

          //create the engine;
          CreateRoutingEngine(_building, false);
     }


     //restore the door states
     for(auto&& t:_building->GetAllTransitions())
     {
          if (doors_states[t.second->GetID()])
          {
               t.second->Open();
          }
          else
          {
               t.second->Close();
          }
     }
     Log->Write("INFO: \tdone");
}


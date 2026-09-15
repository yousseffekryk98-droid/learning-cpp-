#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
using namespace std;struct Ticket{string plate,type,entry;};class ParkingLot{size_t capacity=20;map<string,Ticket>active;double hourly=10;static string stamp(){auto n=chrono::system_clock::to_time_t(chrono::system_clock::now());tm lt=*localtime(&n);ostringstream o;o<<put_time(&lt,"%Y-%m-%d %H:%M:%S");return o.str();}void log(const string&s){ofstream("parking.log",ios::app)<<stamp()<<" | "<<s<<'\n';}public:void enter(){if(active.size()>=capacity){cout<<"Lot full.\n";return;}string p,t;cout<<"Plate: ";cin>>p;if(active.count(p))return;cout<<"Type Car/Bike/Truck: ";cin>>t;active[p]={p,t,stamp()};log("ENTRY "+p+" "+t);}void exitVehicle(){string p;double h;cout<<"Plate: ";cin>>p;auto it=active.find(p);if(it==active.end())return;cout<<"Hours: ";cin>>h;double f=it->second.type=="Truck"?1.5:(it->second.type=="Bike"?.5:1.0);double fee=max(1.0,h)*hourly*f;cout<<"Fee: "<<fee<<'\n';log("EXIT "+p+" fee="+to_string(fee));active.erase(it);}void status()const{cout<<"Capacity "<<capacity<<", occupied "<<active.size()<<", available "<<capacity-active.size()<<'\n';for(auto&[p,t]:active)cout<<p<<" | "<<t.type<<" | "<<t.entry<<'\n';}void run(){for(;;){cout<<"\nParking Lot\n1 Entry 2 Exit 3 Status 0 Quit\n";int c;if(!(cin>>c)||c==0)break;if(c==1)enter();else if(c==2)exitVehicle();else if(c==3)status();}}};int main(){ParkingLot{}.run();}

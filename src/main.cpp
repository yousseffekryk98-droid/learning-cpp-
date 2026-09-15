#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket=SOCKET;const Socket INVALID_SOCK=INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket=int;const Socket INVALID_SOCK=-1;
#endif
using namespace std;
static void netInit(){
#ifdef _WIN32
WSADATA d{};if(WSAStartup(MAKEWORD(2,2),&d)!=0)throw runtime_error("WSAStartup failed");
#endif
}static void netDone(){
#ifdef _WIN32
WSACleanup();
#endif
}static void closeSock(Socket s){if(s==INVALID_SOCK)return;
#ifdef _WIN32
closesocket(s);
#else
close(s);
#endif
}static bool sendLine(Socket s,const string&t){string x=t+"\n";size_t p=0;while(p<x.size()){int n=send(s,x.data()+p,(int)(x.size()-p),0);if(n<=0)return false;p+=(size_t)n;}return true;}static bool recvLine(Socket s,string&o){o.clear();char c;while(o.size()<16384){int n=recv(s,&c,1,0);if(n<=0)return false;if(c=='\n')return true;if(c!='\r')o+=c;}return false;}
struct Vec2{float x=0,y=0;};class Entity{protected:int id_;Vec2 pos_,vel_;int health_=100;bool active_=true;public:Entity(int id,Vec2 p):id_(id),pos_(p){}virtual~Entity()=default;int id()const{return id_;}Vec2 pos()const{return pos_;}bool active()const{return active_;}virtual string kind()const=0;virtual void update(float dt){pos_.x+=vel_.x*dt;pos_.y+=vel_.y*dt;pos_.x=max(0.f,min(19.f,pos_.x));pos_.y=max(0.f,min(9.f,pos_.y));}virtual string serialize()const{ostringstream o;o<<kind()<<','<<id_<<','<<pos_.x<<','<<pos_.y<<','<<health_;return o.str();}void damage(int d){health_-=d;if(health_<=0)active_=false;}};class Player:public Entity{string username_;int score_=0;public:Player(int id,string u,Vec2 p):Entity(id,p),username_(move(u)){}string kind()const override{return"PLAYER";}void moveBy(float x,float y){pos_.x=max(0.f,min(19.f,pos_.x+x));pos_.y=max(0.f,min(9.f,pos_.y+y));}void score(int x){score_+=x;}string serialize()const override{ostringstream o;o<<kind()<<','<<id_<<','<<username_<<','<<pos_.x<<','<<pos_.y<<','<<health_<<','<<score_;return o.str();}};class Obstacle:public Entity{public:Obstacle(int id,Vec2 p):Entity(id,p){}string kind()const override{return"OBSTACLE";}void update(float)override{}};class Projectile:public Entity{int owner_;public:Projectile(int id,int owner,Vec2 p):Entity(id,p),owner_(owner){vel_={6,0};}string kind()const override{return"PROJECTILE";}int owner()const{return owner_;}string serialize()const override{ostringstream o;o<<kind()<<','<<id_<<','<<owner_<<','<<pos_.x<<','<<pos_.y;return o.str();}};
class EventManager{mutex m;vector<function<void(const string&)>>listeners;public:void subscribe(function<void(const string&)>f){lock_guard<mutex>g(m);listeners.push_back(move(f));}void emit(const string&e){lock_guard<mutex>g(m);for(auto&f:listeners)f(e);}};class GameWorld{map<int,shared_ptr<Entity>>entities;int nextId=1000;EventManager&events;public:explicit GameWorld(EventManager&e):events(e){entities[nextId]=make_shared<Obstacle>(nextId,{10,5});++nextId;}shared_ptr<Player>addPlayer(int id,string name){auto p=make_shared<Player>(id,move(name),Vec2{float(1+id%18),float(1+id%8)});entities[id]=p;events.emit("PlayerJoinedEvent");return p;}shared_ptr<Player>player(int id){auto it=entities.find(id);return it==entities.end()?nullptr:dynamic_pointer_cast<Player>(it->second);}void removePlayer(int id){entities.erase(id);events.emit("PlayerLeftEvent");}void shoot(int id){auto p=player(id);if(p)entities[nextId]=make_shared<Projectile>(nextId++,id,p->pos());}void update(float dt){for(auto&[id,e]:entities)e->update(dt);vector<int>erase;for(auto&[a,ea]:entities)for(auto&[b,eb]:entities)if(a<b&&ea->active()&&eb->active()){auto pa=ea->pos(),pb=eb->pos();if(hypot(pa.x-pb.x,pa.y-pb.y)<.55){auto pr=dynamic_pointer_cast<Projectile>(ea);auto pl=dynamic_pointer_cast<Player>(eb);if(!pr){pr=dynamic_pointer_cast<Projectile>(eb);pl=dynamic_pointer_cast<Player>(ea);}if(pr&&pl&&pr->owner()!=pl->id()){pl->damage(25);if(auto shooter=player(pr->owner()))shooter->score(10);erase.push_back(pr->id());events.emit("CollisionEvent");}}}for(int id:erase)entities.erase(id);}string serialize()const{ostringstream o;bool f=true;for(auto&[id,e]:entities){if(!f)o<<';';f=false;o<<e->serialize();}return o.str();}void save()const{ofstream("game_state.txt")<<serialize()<<'\n';}};
class Command{public:virtual~Command()=default;virtual void execute(GameWorld&,int)=0;};class MoveCommand:public Command{float x,y;public:MoveCommand(float a,float b):x(a),y(b){}void execute(GameWorld&w,int id)override{if(auto p=w.player(id))p->moveBy(x,y);}};class AttackCommand:public Command{public:void execute(GameWorld&w,int id)override{w.shoot(id);}};class Engine{EventManager events;GameWorld world_{events};mutex m;Engine(){events.subscribe([](const string&e){ofstream("game_events.log",ios::app)<<e<<'\n';});}public:static Engine&instance(){static Engine e;return e;}mutex&mutexRef(){return m;}GameWorld&world(){return world_;}void exec(unique_ptr<Command>c,int id){lock_guard<mutex>g(m);c->execute(world_,id);}void tick(float dt){lock_guard<mutex>g(m);world_.update(dt);}string state(){lock_guard<mutex>g(m);return world_.serialize();}void save(){lock_guard<mutex>g(m);world_.save();}};
static void render(const string&s){vector<string>g(10,string(20,'.'));stringstream all(s);string r;while(getline(all,r,';')){stringstream ss(r);vector<string>p;string x;while(getline(ss,x,','))p.push_back(x);try{int ix,iy;char ch;if(p[0]=="PLAYER"){ch='P';ix=(int)stof(p[3]);iy=(int)stof(p[4]);}else if(p[0]=="PROJECTILE"){ch='o';ix=(int)stof(p[3]);iy=(int)stof(p[4]);}else{ch='#';ix=(int)stof(p[2]);iy=(int)stof(p[3]);}if(ix>=0&&ix<20&&iy>=0&&iy<10)g[iy][ix]=ch;}catch(...){}}cout<<"\n";for(auto&row:g)cout<<row<<'\n';}
class Server{Socket listener=INVALID_SOCK;atomic<bool>run_{true};mutex cm,sm;map<int,Socket>clients;atomic<int>next{1};vector<thread>threads;void broadcast(const string&m){lock_guard<mutex>g(cm);lock_guard<mutex>s(sm);for(auto&[id,fd]:clients)sendLine(fd,m);}void clientLoop(int id,Socket s){sendLine(s,"WELCOME "+to_string(id));string line;while(run_&&recvLine(s,line)){if(line.rfind("MOVE ",0)==0){istringstream in(line.substr(5));float x,y;if(in>>x>>y)Engine::instance().exec(make_unique<MoveCommand>(x,y),id);}else if(line=="ATTACK")Engine::instance().exec(make_unique<AttackCommand>(),id);else if(line=="QUIT")break;}{lock_guard<mutex>g(cm);clients.erase(id);}{lock_guard<mutex>g(Engine::instance().mutexRef());Engine::instance().world().removePlayer(id);}closeSock(s);}public:void run(unsigned short port){netInit();listener=socket(AF_INET,SOCK_STREAM,0);int one=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,(char*)&one,sizeof(one));sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_ANY);a.sin_port=htons(port);if(bind(listener,(sockaddr*)&a,sizeof(a))<0||listen(listener,16)<0)throw runtime_error("bind/listen failed");cout<<"Server on "<<port<<'\n';thread game([&]{while(run_){Engine::instance().tick(.1f);broadcast("STATE "+Engine::instance().state());this_thread::sleep_for(chrono::milliseconds(100));}});while(run_){sockaddr_in c{};
#ifdef _WIN32
int n=sizeof(c);
#else
socklen_t n=sizeof(c);
#endif
Socket s=accept(listener,(sockaddr*)&c,&n);if(s==INVALID_SOCK)break;int id=next++;{lock_guard<mutex>g(cm);clients[id]=s;}{lock_guard<mutex>g(Engine::instance().mutexRef());Engine::instance().world().addPlayer(id,"Player"+to_string(id));}threads.emplace_back(&Server::clientLoop,this,id,s);}run_=false;closeSock(listener);if(game.joinable())game.join();for(auto&t:threads)if(t.joinable())t.join();Engine::instance().save();netDone();}};class Client{Socket s=INVALID_SOCK;atomic<bool>run_{true};public:void run(const string&host,unsigned short port){netInit();s=socket(AF_INET,SOCK_STREAM,0);sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(port);if(inet_pton(AF_INET,host.c_str(),&a.sin_addr)!=1||connect(s,(sockaddr*)&a,sizeof(a))<0)throw runtime_error("connect failed");thread rx([&]{string l;while(run_&&recvLine(s,l)){if(l.rfind("STATE ",0)==0)render(l.substr(6));else cout<<l<<'\n';}run_=false;});char c;while(run_&&cin>>c){if(c=='q'){sendLine(s,"QUIT");break;}if(c=='w')sendLine(s,"MOVE 0 -1");else if(c=='s')sendLine(s,"MOVE 0 1");else if(c=='a')sendLine(s,"MOVE -1 0");else if(c=='d')sendLine(s,"MOVE 1 0");else if(c=='f')sendLine(s,"ATTACK");}run_=false;closeSock(s);if(rx.joinable())rx.join();netDone();}};int main(int argc,char**argv){try{if(argc>=2&&string(argv[1])=="server")Server{}.run(argc>=3?(unsigned short)stoi(argv[2]):4040);else if(argc>=2&&string(argv[1])=="client")Client{}.run(argc>=3?argv[2]:"127.0.0.1",argc>=4?(unsigned short)stoi(argv[3]):4040);else cout<<"Usage: app server [port] | app client [IPv4] [port]\nControls: w/a/s/d move, f fire, q quit\n";}catch(exception&e){cerr<<e.what()<<'\n';return 1;}}

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
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
}static bool sendLine(Socket s,const string&x){string b=x+"\n";size_t p=0;while(p<b.size()){int n=send(s,b.data()+p,(int)(b.size()-p),0);if(n<=0)return false;p+=(size_t)n;}return true;}static bool recvLine(Socket s,string&o){o.clear();char c;while(o.size()<32768){int n=recv(s,&c,1,0);if(n<=0)return false;if(c=='\n')return true;if(c!='\r')o+=c;}return false;}static string now(){time_t t=time(nullptr);char b[32];strftime(b,sizeof(b),"%Y-%m-%d %H:%M:%S",localtime(&t));return b;}
class Message{long long id;string sender,content,target,stamp;public:Message(long long i,string s,string c,string t=""):id(i),sender(move(s)),content(move(c)),target(move(t)),stamp(now()){}string format(const string&type)const{ostringstream o;o<<'['<<stamp<<"] #"<<id<<' '<<type<<" <"<<sender<<">";if(!target.empty())o<<" -> "<<target;o<<": "<<content;return o.str();}};class ChatRoom{int id;string name;set<string>members;public:ChatRoom(int i,string n):id(i),name(move(n)){}int getId()const{return id;}const string&getName()const{return name;}void add(const string&u){members.insert(u);}void remove(const string&u){members.erase(u);}bool has(const string&u)const{return members.count(u);}const set<string>&all()const{return members;}};struct User{string username,password;Socket socket=INVALID_SOCK;bool online=false;set<string>rooms;};
class Server{Socket listener=INVALID_SOCK;atomic<bool>running{true};mutex m,sendM,logM;unordered_map<string,User>users;map<string,ChatRoom>rooms;atomic<long long>nextMsg{1};atomic<int>nextRoom{1};vector<thread>threads;void load(){ifstream f("users.db");string l;while(getline(f,l)){auto p=l.find('|');if(p!=string::npos)users[l.substr(0,p)]={l.substr(0,p),l.substr(p+1)};}}void saveUsers(){ofstream f("users.db");for(auto&[n,u]:users)f<<n<<'|'<<u.password<<'\n';}void log(const string&s){lock_guard<mutex>g(logM);ofstream("chat.log",ios::app)<<s<<'\n';}void sendSafe(Socket s,const string&x){lock_guard<mutex>g(sendM);sendLine(s,x);}void broadcast(const string&x){lock_guard<mutex>g(m);for(auto&[n,u]:users)if(u.online)sendSafe(u.socket,x);}bool auth(Socket s,string&user){string line;if(!recvLine(s,line))return false;stringstream ss(line);string op,pass;getline(ss,op,'|');getline(ss,user,'|');getline(ss,pass);lock_guard<mutex>g(m);if(op=="REGISTER"){if(users.count(user)){sendSafe(s,"AUTH_FAIL username exists");return false;}users[user]={user,pass};saveUsers();}else if(op=="LOGIN"){if(!users.count(user)||users[user].password!=pass){sendSafe(s,"AUTH_FAIL wrong credentials");return false;}}else return false;if(users[user].online){sendSafe(s,"AUTH_FAIL duplicate login");return false;}users[user].online=true;users[user].socket=s;sendSafe(s,"AUTH_OK");return true;}void handle(const string&u,const string&line){Socket s;{lock_guard<mutex>g(m);s=users[u].socket;}if(line=="/users"){lock_guard<mutex>g(m);string x="ONLINE:";for(auto&[n,v]:users)if(v.online)x+=' '+n;sendSafe(s,x);return;}if(line.rfind("/msg ",0)==0){istringstream in(line.substr(5));string target,text;in>>target;getline(in,text);lock_guard<mutex>g(m);if(users.count(target)&&users[target].online){string x=Message(nextMsg++,u,text,target).format("PRIVATE");sendSafe(users[target].socket,x);sendSafe(s,x);log(x);}return;}if(line.rfind("/create ",0)==0){string r=line.substr(8);lock_guard<mutex>g(m);if(!rooms.count(r)){auto it=rooms.emplace(r,ChatRoom(nextRoom++,r));it.first->second.add(u);users[u].rooms.insert(r);sendSafe(s,"Created "+r);}return;}if(line.rfind("/join ",0)==0){string r=line.substr(6);lock_guard<mutex>g(m);auto it=rooms.find(r);if(it!=rooms.end()){it->second.add(u);users[u].rooms.insert(r);sendSafe(s,"Joined "+r);}return;}if(line.rfind("/leave ",0)==0){string r=line.substr(7);lock_guard<mutex>g(m);auto it=rooms.find(r);if(it!=rooms.end())it->second.remove(u);users[u].rooms.erase(r);return;}if(line.rfind("/room ",0)==0){istringstream in(line.substr(6));string r,text;in>>r;getline(in,text);lock_guard<mutex>g(m);auto it=rooms.find(r);if(it!=rooms.end()&&it->second.has(u)){string x=Message(nextMsg++,u,text,r).format("GROUP");for(auto&member:it->second.all())if(users.count(member)&&users[member].online)sendSafe(users[member].socket,x);log(x);}return;}string x=Message(nextMsg++,u,line).format("PUBLIC");log(x);broadcast(x);}void client(Socket s){string u;if(!auth(s,u)){closeSock(s);return;}broadcast("[SYSTEM] "+u+" joined");sendSafe(s,"Commands: /users /msg USER TEXT /create ROOM /join ROOM /leave ROOM /room ROOM TEXT /quit");string line;while(running&&recvLine(s,line)){if(line=="/quit")break;handle(u,line);} {lock_guard<mutex>g(m);users[u].online=false;users[u].socket=INVALID_SOCK;}broadcast("[SYSTEM] "+u+" left");closeSock(s);}public:void run(unsigned short port){netInit();load();listener=socket(AF_INET,SOCK_STREAM,0);int one=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,(char*)&one,sizeof(one));sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_ANY);a.sin_port=htons(port);if(bind(listener,(sockaddr*)&a,sizeof(a))<0||listen(listener,32)<0)throw runtime_error("bind/listen failed");cout<<"Chat server on "<<port<<'\n';while(running){sockaddr_in c{};
#ifdef _WIN32
int n=sizeof(c);
#else
socklen_t n=sizeof(c);
#endif
Socket s=accept(listener,(sockaddr*)&c,&n);if(s==INVALID_SOCK)break;threads.emplace_back(&Server::client,this,s);}closeSock(listener);for(auto&t:threads)if(t.joinable())t.join();saveUsers();netDone();}};class Client{Socket s=INVALID_SOCK;atomic<bool>run_{true};public:void run(const string&host,unsigned short port){netInit();s=socket(AF_INET,SOCK_STREAM,0);sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(port);if(inet_pton(AF_INET,host.c_str(),&a.sin_addr)!=1||connect(s,(sockaddr*)&a,sizeof(a))<0)throw runtime_error("connect failed");string mode,user,pass;cout<<"register or login: ";getline(cin,mode);cout<<"username: ";getline(cin,user);cout<<"password: ";getline(cin,pass);transform(mode.begin(),mode.end(),mode.begin(),::toupper);sendLine(s,mode+"|"+user+"|"+pass);string first;if(!recvLine(s,first))return;cout<<first<<'\n';if(first!="AUTH_OK")return;thread rx([&]{string l;while(run_&&recvLine(s,l))cout<<l<<'\n';run_=false;});string line;while(run_&&getline(cin,line)){sendLine(s,line);if(line=="/quit")break;}run_=false;closeSock(s);if(rx.joinable())rx.join();netDone();}};int main(int argc,char**argv){try{if(argc>=2&&string(argv[1])=="server")Server{}.run(argc>=3?(unsigned short)stoi(argv[2]):5050);else if(argc>=2&&string(argv[1])=="client")Client{}.run(argc>=3?argv[2]:"127.0.0.1",argc>=4?(unsigned short)stoi(argv[3]):5050);else cout<<"Usage: app server [port] | app client [IPv4] [port]\n";}catch(exception&e){cerr<<e.what()<<'\n';return 1;}}

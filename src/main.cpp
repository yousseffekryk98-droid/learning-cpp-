#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;
class Product{
    int id{};string name;double price{};int qty{};
public:Product()=default;Product(int i,string n,double p,int q):id(i),name(move(n)),price(p),qty(q){if(p<0||q<0)throw invalid_argument("negative values not allowed");}
    int getId()const{return id;}const string& getName()const{return name;}double getPrice()const{return price;}int getQty()const{return qty;}double value()const{return price*qty;}
    void setQty(int q){if(q<0)throw invalid_argument("quantity cannot be negative");qty=q;}string data()const{return to_string(id)+"|"+name+"|"+to_string(price)+"|"+to_string(qty);} };
class Inventory{
    vector<Product> items;string file="inventory.txt";Product* find(int id){for(auto&p:items)if(p.getId()==id)return &p;return nullptr;}
public:void load(){ifstream in(file);string l;while(getline(in,l)){stringstream s(l);string a,b,c,d;getline(s,a,'|');getline(s,b,'|');getline(s,c,'|');getline(s,d);if(!a.empty())items.emplace_back(stoi(a),b,stod(c),stoi(d));}}
    void save()const{ofstream out(file);for(auto&p:items)out<<p.data()<<'\n';}
    void add(){int id,q;double p;string n;cout<<"ID: ";cin>>id;if(find(id)){cout<<"Duplicate ID.\n";return;}cout<<"Name: ";cin>>ws;getline(cin,n);cout<<"Price: ";cin>>p;cout<<"Quantity: ";cin>>q;try{items.emplace_back(id,n,p,q);}catch(exception&e){cout<<e.what()<<'\n';}}
    void update(){int id,q;cout<<"ID: ";cin>>id;auto*p=find(id);if(!p){cout<<"Not found.\n";return;}cout<<"New quantity: ";cin>>q;try{p->setQty(q);}catch(exception&e){cout<<e.what()<<'\n';}}
    void removeItem(){int id;cout<<"ID: ";cin>>id;items.erase(remove_if(items.begin(),items.end(),[&](auto&p){return p.getId()==id;}),items.end());}
    void display()const{double total=0;cout<<left<<setw(7)<<"ID"<<setw(25)<<"Name"<<setw(12)<<"Price"<<setw(10)<<"Qty"<<"Value\n";for(auto&p:items){cout<<left<<setw(7)<<p.getId()<<setw(25)<<p.getName()<<setw(12)<<p.getPrice()<<setw(10)<<p.getQty()<<p.value();if(p.getQty()<5)cout<<"  LOW STOCK";cout<<'\n';total+=p.value();}cout<<"Total inventory value: "<<total<<'\n';}
    void run(){load();for(;;){cout<<"\nInventory\n1 Add 2 Update quantity 3 Remove 4 Display 0 Exit\nChoice: ";int c;if(!(cin>>c))break;if(c==0){save();break;}if(c==1)add();else if(c==2)update();else if(c==3)removeItem();else if(c==4)display();save();}}
};int main(){Inventory{}.run();}

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

class Student {
    int id{}; string name; vector<double> grades;
public:
    Student()=default;
    Student(int i,string n):id(i),name(move(n)){}
    int getId() const { return id; }
    const string& getName() const { return name; }
    const vector<double>& getGrades() const { return grades; }
    void setName(string n){ name=move(n); }
    void addGrade(double g){ if(g<0||g>100) throw invalid_argument("grade must be 0..100"); grades.push_back(g); }
    void updateGrade(size_t idx,double g){ if(idx>=grades.size()||g<0||g>100) throw invalid_argument("invalid grade update"); grades[idx]=g; }
    double average() const { return grades.empty()?0.0:accumulate(grades.begin(),grades.end(),0.0)/grades.size(); }
    string standing() const { double a=average(); if(a>=90)return "A / Excellent"; if(a>=80)return "B / Very Good"; if(a>=70)return "C / Good"; if(a>=60)return "D / Pass"; return "F / At Risk"; }
    string serialize() const { ostringstream o; o<<id<<'|'<<name<<'|'; for(size_t i=0;i<grades.size();++i){ if(i)o<<','; o<<grades[i]; } return o.str(); }
    static Student parse(const string& line){ string a,b,c; stringstream ss(line); getline(ss,a,'|');getline(ss,b,'|');getline(ss,c); Student s(stoi(a),b); string x; stringstream gs(c); while(getline(gs,x,',')) if(!x.empty()) s.grades.push_back(stod(x)); return s; }
};

class StudentManager {
    vector<Student> students; string file="students.txt";
    Student* find(int id){ for(auto& s:students) if(s.getId()==id) return &s; return nullptr; }
public:
    void load(){ ifstream in(file); string line; while(getline(in,line)){ if(line.empty())continue; try{students.push_back(Student::parse(line));}catch(...){}} }
    void save() const { ofstream out(file); for(const auto& s:students) out<<s.serialize()<<'\n'; }
    void add(){ int id; string name; cout<<"ID: ";cin>>id; if(find(id)){cout<<"Duplicate ID.\n";return;} cout<<"Name: ";cin>>ws;getline(cin,name); students.emplace_back(id,name); }
    void grades(){ int id; cout<<"Student ID: ";cin>>id; auto*s=find(id); if(!s){cout<<"Not found.\n";return;} int ch; cout<<"1 Add grade  2 Update grade: ";cin>>ch; double g; if(ch==1){cout<<"Grade: ";cin>>g; try{s->addGrade(g);}catch(const exception&e){cout<<e.what()<<'\n';}} else if(ch==2){size_t i;cout<<"Grade index (1-based): ";cin>>i;cout<<"New grade: ";cin>>g; try{s->updateGrade(i-1,g);}catch(const exception&e){cout<<e.what()<<'\n';}} }
    void removeStudent(){ int id;cout<<"ID to delete: ";cin>>id; auto old=students.size(); students.erase(remove_if(students.begin(),students.end(),[&](const Student&s){return s.getId()==id;}),students.end()); cout<<(students.size()<old?"Deleted.\n":"Not found.\n"); }
    void showOne(){ int id;cout<<"ID: ";cin>>id; auto*s=find(id); if(!s){cout<<"Not found.\n";return;} cout<<s->getId()<<" | "<<s->getName()<<" | Avg "<<fixed<<setprecision(2)<<s->average()<<" | "<<s->standing()<<'\n'; }
    void showAll(){ auto copy=students; sort(copy.begin(),copy.end(),[](auto&a,auto&b){return a.average()>b.average();}); cout<<left<<setw(8)<<"ID"<<setw(28)<<"Name"<<setw(10)<<"Average"<<"Standing\n"; for(const auto&s:copy) cout<<left<<setw(8)<<s.getId()<<setw(28)<<s.getName()<<setw(10)<<fixed<<setprecision(2)<<s.average()<<s.standing()<<'\n'; }
    void run(){ load(); for(;;){cout<<"\nStudent Grade Manager\n1 Add student\n2 Add/update grades\n3 Search by ID\n4 Display all (sorted)\n5 Delete student\n6 Save\n0 Exit\nChoice: ";int c;if(!(cin>>c))break; if(c==0){save();break;} switch(c){case 1:add();break;case 2:grades();break;case 3:showOne();break;case 4:showAll();break;case 5:removeStudent();break;case 6:save();cout<<"Saved.\n";break;default:cout<<"Invalid.\n";}} }
};
int main(){ try{StudentManager{}.run();}catch(const exception&e){cerr<<"Fatal: "<<e.what()<<'\n';return 1;} }

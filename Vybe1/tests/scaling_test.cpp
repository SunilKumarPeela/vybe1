#include "point_core.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
    try {
        const auto root = std::filesystem::temp_directory_path() / ("point-scaling-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        point::ensure_directories(root);
        struct Cleanup { std::filesystem::path p; ~Cleanup() { std::error_code e; std::filesystem::remove_all(p,e); } } cleanup{root};
        const auto a=root/"people.csv", b=root/"departments.csv";
        { std::ofstream f(a); f << "Employee ID,Username\n001,alex\n"; }
        { std::ofstream f(b); f << "Employee ID,Department\n"; for(int i=0;i<20;++i) f<<"001,Dept"<<i<<"\n"; }
        point::QueryRequest q; q.lookup_field="Username";q.lookup_value="alex";q.output_fields={"Username","Department"};
        point::Engine engine;engine.load_files({a,b});
        const auto result=engine.query(q);
        check(result.rows.size()==20,"One-to-many results were lost");
        point::Limits bounded;bounded.max_join_work=3;
        point::Engine work_limited(bounded);work_limited.load_files({a,b});
        bool rejected=false;try{work_limited.query(q);}catch(const std::runtime_error& e){rejected=std::string(e.what()).find("work limit")!=std::string::npos;}
        check(rejected,"Traversal work budget did not stop expansion");
        bounded.max_join_work=10000;bounded.max_result_rows=5;
        point::Engine rows_limited(bounded);rows_limited.load_files({a,b});
        rejected=false;try{rows_limited.query(q);}catch(const std::runtime_error& e){rejected=std::string(e.what()).find("match limit")!=std::string::npos;}
        check(rejected,"Candidate cardinality limit did not stop expansion");
        q.output_fields={"Username"};
        check(work_limited.query(q).rows.size()==1,"Complete source unnecessarily expanded");
        // Sparse large log: appending must use the final record and preserve the prefix.
        point::append_audit(root,"FIRST","seed");
        std::filesystem::path log;
        for(const auto& entry:std::filesystem::directory_iterator(root/"Logs")) if(entry.path().extension()==".log") log=entry.path();
        std::string row; {std::ifstream in(log);std::getline(in,row);}
        const auto hash=row.substr(row.find_last_of('\t')+1);
        {std::ofstream out(log,std::ios::binary|std::ios::trunc);out.seekp(64*1024*1024);out<<'\n'<<row<<'\n';}
        const auto previous_size=std::filesystem::file_size(log);
        point::append_audit(root,"AFTER","large log");
        std::ifstream in(log,std::ios::binary);in.seekg(static_cast<std::streamoff>(previous_size));std::string appended;std::getline(in,appended);
        const auto end=appended.find_last_of('\t'), start=appended.find_last_of('\t',end-1);
        check(appended.substr(start+1,end-start-1)==hash,"Large-log append lost the previous hash");in.close();
        std::cout<<"PASS: complete 20-row join, traversal budget, candidate bound, complete-source pruning, 64 MiB audit-tail append.\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}

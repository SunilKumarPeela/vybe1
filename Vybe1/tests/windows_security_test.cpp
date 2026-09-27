#include "point_compliance.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#define NOMINMAX
#include <windows.h>
#include <sddl.h>
#include <stdexcept>
int main() {
    const auto root=std::filesystem::temp_directory_path()/L"point-windows-security-test";
    std::filesystem::create_directories(root);
    auto policy=[&](const char* enforce,const char* retention) {
        std::ofstream out(root/"point-security.conf");
        out<<"enforce_windows_groups="<<enforce<<"\nallowed_windows_groups=Point Users\nexport_windows_groups=Point Exporters\nexport_retention_days="<<retention<<"\nworkspace_retention_days=30\nlog_retention_days=365\n";
    };
    auto read_policy = [&]() {
        std::ifstream input(root/"point-security.conf");
        const std::string text{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
        return point::compliance::parse_policy_text(text);
    };
    policy("true","30");assert(read_policy().enforce_windows_groups);
    for(const auto* value:{"tru","false",""}) {
        policy(value,"30");bool denied=false;try{read_policy();}catch(...){denied=true;}assert(denied);
    }
    policy("true","30days");bool denied=false;try{read_policy();}catch(...){denied=true;}assert(denied);
    policy("true","30");{std::ofstream out(root/"point-security.conf",std::ios::app);out<<"enforce_windows_groups=true\n";}
    denied=false;try{read_policy();}catch(...){denied=true;}assert(denied);
    // Reproduce the old owner-only ACL under an elevated run, then verify the
    // corrected ACL with Administrators disabled in the access token.
    const auto secure = root / L"data";
    std::filesystem::create_directories(secure / L"Workspace");
    HANDLE process_token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &process_token))
        throw std::runtime_error("Cannot open test token");
    struct TokenGuard { HANDLE h; ~TokenGuard() { if(h) CloseHandle(h); } } process_guard{process_token};
    TOKEN_ELEVATION elevation{}; DWORD bytes_needed=0;
    if (!GetTokenInformation(process_token,TokenElevation,&elevation,sizeof(elevation),&bytes_needed))
        throw std::runtime_error("Cannot inspect test elevation");
    if (elevation.TokenIsElevated) {
        PSECURITY_DESCRIPTOR old_acl=nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"O:BAD:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;OW)",SDDL_REVISION_1,&old_acl,nullptr))
            throw std::runtime_error("Cannot create legacy test ACL");
        const BOOL applied=SetFileSecurityW((secure/L"Workspace").c_str(),OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,old_acl);
        LocalFree(old_acl);
        if (!applied) throw std::runtime_error("Cannot reproduce legacy test ACL");
    }
    point::compliance::harden_data_directories(secure);
    BYTE admin_sid[SECURITY_MAX_SID_SIZE]{};DWORD admin_size=sizeof(admin_sid);
    if (!CreateWellKnownSid(WinBuiltinAdministratorsSid,nullptr,admin_sid,&admin_size)) throw std::runtime_error("Cannot build admin SID");
    SID_AND_ATTRIBUTES disabled{admin_sid,0};HANDLE restricted=nullptr;
    if (!CreateRestrictedToken(process_token,DISABLE_MAX_PRIVILEGE,1,&disabled,0,nullptr,0,nullptr,&restricted))
        throw std::runtime_error("Cannot create non-admin test token");
    TokenGuard restricted_guard{restricted};
    if (!ImpersonateLoggedOnUser(restricted)) throw std::runtime_error("Cannot use non-admin test token");
    {
        struct Revert { ~Revert() { RevertToSelf(); } } revert;
        point::compliance::harden_data_directories(secure);
        std::ofstream probe(secure/L"Workspace"/L"normal-user.txt");probe<<"normal token write";probe.close();
        if(!probe) throw std::runtime_error("Normal token cannot write after repair");
    }
    std::cout<<"PASS: data ACL repair, reopen and write with Administrators disabled.\n";
    const auto encrypted=root/L"protected.dat";
    const std::string clear="CONFIDENTIAL_POINT_TEST_SECRET_ABCDEF1234567890";
    point::compliance::write_user_protected_file(encrypted,clear);
    assert(point::compliance::read_user_protected_file(encrypted)==clear);
    std::ifstream input(encrypted,std::ios::binary);
    const std::string bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    assert(bytes.find(clear)==std::string::npos);input.close();
    std::filesystem::remove_all(root);
    std::cout<<"PASS: actual Windows policy parsing and DPAPI encrypted file round trip.\n";
}


#include <set>

#if defined(_WIN32)
#  define USE_LANMAN
#elif !defined(__rtems__) && !defined(vxWorks)
#  define USE_UNIX_GROUPS
#endif

/* conditionally include any system headers */
#if defined(USE_UNIX_GROUPS)

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>

#elif defined(USE_LANMAN)

#include <stdlib.h>
#include <windows.h>
#include <lm.h>

#endif

#define epicsExportSharedSymbols
#include <pv/security.h>

namespace epics {
namespace pvAccess {

#if defined(USE_UNIX_GROUPS)

void osdGetRoles(const std::string& account, PeerInfo::roles_t& roles)
{
    passwd *user = getpwnam(account.c_str());
    if(!user)
        return; // don't know who this is

    typedef std::set<gid_t> gids_t;
    gids_t gids;

    gids.insert(user->pw_gid); // include primary group

    // include supplementary groups
    {
        std::vector<gid_t> gtemp(16);
        int gcount = int(gtemp.size());

        if(getgrouplist(user->pw_name, user->pw_gid, &gtemp[0], &gcount)==-1 && gcount>=0 && gcount<=NGROUPS_MAX) {
            gtemp.resize(gcount);
            // try again.  This time if we fail, then there is some other error
            getgrouplist(user->pw_name, user->pw_gid, &gtemp[0], &gcount);
        }
        gtemp.resize(std::min(gcount, NGROUPS_MAX));

        for(size_t i=0, N=gtemp.size(); i<N; i++)
            gids.insert(gtemp[i]);
    }

    // map GIDs to names
    for(gids_t::iterator it(gids.begin()), end(gids.end()); it!=end; it++) {
        assert((*it)!=0); // 0 indicates a logic error above

        group* gr = getgrgid(*it);
        if(!gr)
            continue;
        roles.insert(gr->gr_name);
    }
}

#elif defined(USE_LANMAN)

void osdGetRoles(const std::string& account, PeerInfo::roles_t& roles)
{
    NET_API_STATUS sts;
    LPLOCALGROUP_USERS_INFO_0 pinfo = NULL;
    DWORD ninfo = 0, nmaxinfo = 0;
    std::vector<wchar_t> wbuf;

    {
        size_t N = mbstowcs(NULL, account.c_str(), 0);
        if(N==size_t(-1))
            return; // username has invalid MB char
        wbuf.resize(N+1);
        N = mbstowcs(&wbuf[0], account.c_str(), account.size());
        assert(N+1==wbuf.size());
        wbuf[N] = 0; // paranoia
    }

    // this call may involve network I/O
    sts = NetUserGetLocalGroups(NULL, &wbuf[0], 0,
                                LG_INCLUDE_INDIRECT,
                                (LPBYTE*)&pinfo,
                                MAX_PREFERRED_LENGTH,
                                &ninfo, &nmaxinfo);

    if(sts!=NERR_Success)
        return; // silently do nothing.

    try {
        std::vector<char> buf;

        for(DWORD i=0; i<ninfo; i++) {
            //std::wstring group(pinfo[i].lgrui0_name);
            size_t N = wcstombs(NULL, pinfo[i].lgrui0_name, 0);
            if(N==size_t(-1))
                continue; // has invalid MB char

            buf.resize(N+1);
            N = wcstombs(&buf[0], pinfo[i].lgrui0_name, buf.size());
            assert(N+1==buf.size());
            buf[N] = 0; // paranoia

            roles.insert(&buf[0]);
        }

        NetApiBufferFree(pinfo);
    }catch(...){
        NetApiBufferFree(pinfo);
        throw;
    }
}

#else

void osdGetRoles(const std::string& account, PeerInfo::roles_t& roles)
{}
#endif

}} // namespace epics::pvAccess

/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <vector>
#include <stdexcept>
#include <iostream>

#include <osiProcess.h>
#include <pv/security.h>

namespace pva = epics::pvAccess;

int main(int argc, char *argv[])
{
    int ret = 0;
    try {
        std::vector<char> name(256u);
        if(osiGetUserName(&name[0], name.size())!=osiGetUserNameSuccess)
            throw std::runtime_error("Unable to determine username");

        name[name.size()-1] = '\0';

        const char *user = argc<=1 ? &name[0] : argv[1];

        std::cout<<"User: "<<user<<"\n";

        pva::PeerInfo::roles_t roles;
        pva::osdGetRoles(user, roles);

        std::cout<<"Groups: \n";
        for(pva::PeerInfo::roles_t::const_iterator it(roles.begin()), end(roles.end());
            it!=end; ++it)
        {
            std::cout<<"  "<<*it<<"\n";
        }

    } catch(std::exception& e) {
        std::cerr<<"Error: "<<e.what()<<"\n";
        ret = 2;
    }
    return ret;
}

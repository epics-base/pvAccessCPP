/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
// The simplest possible PVA get

#include <iostream>

#include "pva/client.h"

int main(int argc, char *argv[])
{
    try {
        if(argc<=1) {
            std::cerr<<"Usage: "<<argv[0]<<" <pvname>\n";
            return 1;
        }

        pvac::ClientProvider provider("pva");

        pvac::ClientChannel channel(provider.connect(argv[1]));

        std::cout<<channel.name()<<" : "<<channel.get()<<"\n";

    }catch(std::exception& e){
        std::cerr<<"Error: "<<e.what()<<"\n";
        return 1;
    }
}

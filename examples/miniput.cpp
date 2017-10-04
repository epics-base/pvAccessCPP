/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
// The simplest possible PVA put

#include <iostream>

#include "pva/client.h"

int main(int argc, char *argv[])
{
    try {
        if(argc<=2) {
            std::cerr<<"Usage: "<<argv[0]<<" <pvname> <value>\n";
            return 1;
        }

        pvac::ClientProvider provider("pva");

        pvac::ClientChannel channel(provider.connect(argv[1]));

        std::cout<<"Before "<<channel.name()<<" : "<<channel.get()<<"\n";

        channel.put()
                .set("value", argv[2])
                .exec();

        std::cout<<"After  "<<channel.name()<<" : "<<channel.get()<<"\n";

    }catch(std::exception& e){
        std::cerr<<"Error: "<<e.what()<<"\n";
        return 1;
    }
}

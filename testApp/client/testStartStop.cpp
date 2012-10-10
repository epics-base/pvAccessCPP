#include <iostream>
#include <pv/clientFactory.h>

int main() {
   std::cout << "to start pvAccess ClientFactory" << std::endl;
   ::epics::pvAccess::ClientFactory::start();
   std::cout << "do nothing after starting pvAccess ClientFactory" << std::endl;

   std::cout << "to stop pvAccess ClientFactory" << std::endl;
   ::epics::pvAccess::ClientFactory::stop();
   std::cout << "finish test" << std::endl;
   return 0;

}

#include <iostream>
#include <pv/clientFactory.h>

#include <stdio.h>
#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <pv/logger.h>

#include <vector>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <pv/event.h>

using namespace std;
using namespace std::tr1;
using namespace epics::pvData;
using namespace epics::pvAccess;

#define COUNT 100000

#define DEFAULT_TIMEOUT 60.0
#define DEFAULT_REQUEST "field(value)"

double timeOut = DEFAULT_TIMEOUT;
string request(DEFAULT_REQUEST);

PVStructure::shared_pointer pvRequest;

void usage (void)
{
   fprintf (stderr, "\nUsage: testGetPerformance [options] <PV name>...\n\n"
   "  -h: Help: Print this message\n"
   "options:\n"
   "  -r <pv request>:   Request, specifies what fields to return and options, default is '%s'\n"
   "  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n\n"
            , DEFAULT_REQUEST, DEFAULT_TIMEOUT);
}


class ChannelGetRequesterImpl : public ChannelGetRequester
{
   private:
   ChannelGet::shared_pointer m_channelGet;
   PVStructure::shared_pointer m_pvStructure;
   BitSet::shared_pointer m_bitSet;
   Event m_event;
   String m_channelName;
   int m_count;

   timeval m_startTime;

   public:

   ChannelGetRequesterImpl(String channelName) : m_channelName(channelName) {};

   virtual String getRequesterName()
   {
       return "ChannelGetRequesterImpl";
   };

   virtual void message(String message,MessageType messageType)
   {
       std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
   }

   virtual void channelGetConnect(const epics::pvData::Status& status,
        ChannelGet::shared_pointer const & channelGet,
        epics::pvData::PVStructure::shared_pointer const & pvStructure, 
        epics::pvData::BitSet::shared_pointer const & bitSet)
   {
       if (status.isSuccess())
       {
           // show warning
           if (!status.isOK())
           {
               std::cout << "[" << m_channelName << "] channel get create: " << status.toString() << std::endl;
           }

           m_channelGet = channelGet;
           m_pvStructure = pvStructure;
           m_bitSet = bitSet;

           m_count = COUNT;

           gettimeofday(&m_startTime, NULL);
           m_channelGet->get(false);
       }
       else
       {
           std::cout << "[" << m_channelName << "] failed to create channel get: " << status.toString() << std::endl;
       }
   }

   virtual void getDone(const epics::pvData::Status& status)
   {
       if (status.isSuccess())
       {
           // show warning
           if (!status.isOK())
           {
               std::cout << "[" << m_channelName << "] channel get: " << status.toString() << std::endl;
           }


           //String str;
           //m_pvStructure->toString(&str);
           //std::cout << str << std::endl;

           //m_event.signal();

           if (--m_count)
               m_channelGet->get(false);
           else
           {
               timeval endTime;
               gettimeofday(&endTime, NULL);


               long seconds, nseconds;
               double duration;
               seconds  = endTime.tv_sec  - m_startTime.tv_sec;
               nseconds = endTime.tv_usec - m_startTime.tv_usec;

               duration = seconds + nseconds/1000000.0;

               printf("%5.6f seconds, %5.3f gets/s \n", duration, COUNT/duration);

               m_event.signal();
           }
       }
       else
       {
           std::cout << "[" << m_channelName << "] failed to get: " << status.toString() << std::endl;
       }

   }

   bool wait(double timeOut)
   {
       return m_event.wait(timeOut);
   }
};

class ChannelRequesterImpl : public ChannelRequester
{
private:
   Event m_event;

public:

   virtual String getRequesterName()
   {
       return "ChannelRequesterImpl";
   };

   virtual void message(String message,MessageType messageType)
   {
       std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
   }

   virtual void channelCreated(const epics::pvData::Status& status,
            Channel::shared_pointer const & channel)
   {
       if (status.isSuccess())
       {
           // show warning
           if (!status.isOK())
           {
               std::cout << "[" << channel->getChannelName() << "] channel create: " << status.toString() << std::endl;
           }
       }
       else
       {
           std::cout << "[" << channel->getChannelName() << "] failed to create a channel: " << status.toString() << std::endl;
       }
   }

   virtual void channelStateChange(Channel::shared_pointer const &channel, Channel::ConnectionState connectionState)
   {
       if (connectionState == Channel::CONNECTED)
       {
           m_event.signal();
       }
   }

   bool waitUntilConnected(double timeOut)
   {
       return m_event.wait(timeOut);
   }
};



int main (int argc, char *argv[])
{
   int opt;                    // getopt() current option

   setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    // Set stdout to line buffering

   while ((opt = getopt(argc, argv, ":hr:w:t")) != -1) {
       switch (opt) {
       case 'h':               // Print usage
           usage();
           return 0;
       case 'w':               // Set CA timeout value
           if(epicsScanDouble(optarg, &timeOut) != 1)
           {
               fprintf(stderr, "'%s' is not a valid timeout value "
                       "- ignored. ('cainfo -h' for help.)\n", optarg);
               timeOut = DEFAULT_TIMEOUT;
           }
           break;
       case 'r':               // Set CA timeout value
           request = optarg;
           break;
       case '?':
           fprintf(stderr,
                   "Unrecognized option: '-%c'. ('testGetPerformance -h' for help.)\n",
                   optopt);
           return 1;
       case ':':
           fprintf(stderr,
                   "Option '-%c' requires an argument. ('testGetPerformance -h' for help.)\n",
                   optopt);
           return 1;
       default :
           usage();
           return 1;
       }
   }

   int nPvs = argc - optind;       // Remaining arg list is PV name
   if (nPvs < 1 || nPvs > 1)
   {
       fprintf(stderr, "No pv name specified. ('testGetPerformance -h' for help.)\n");
       return 1;
   }

   string pvName = argv[optind];      // Copy PV name from command line

   try {
       pvRequest = getCreateRequest()->createRequest(request);
   } catch (std::exception &ex) {
       printf("failed to parse request string: %s\n", ex.what());
       return 1;
   }

   // typedef enum {logLevelInfo, logLevelDebug, logLevelError, errlogFatal} errlogSevEnum;
   SET_LOG_LEVEL(logLevelError);

   bool allOK = 1;

   ClientFactory::start();
   ChannelProvider::shared_pointer provider = getChannelAccess()->getProvider("pvAccess");

   shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new
        ChannelRequesterImpl());
   Channel::shared_pointer channel = provider->createChannel(pvName, channelRequesterImpl);

   if (channelRequesterImpl->waitUntilConnected(5.0))
   {
       shared_ptr<ChannelGetRequesterImpl> getRequesterImpl(new
            ChannelGetRequesterImpl(channel->getChannelName()));
       ChannelGet::shared_pointer channelGet = channel->createChannelGet(getRequesterImpl, pvRequest);

       allOK &= getRequesterImpl->wait(timeOut);

       if (!allOK)
       {
           std::cout << "[" << channel->getChannelName() << "] failed to get all the gets" << std::endl;
       }

       channelGet->destroy();
   }
   else
   {
       allOK = false;
       channel->destroy();
       std::cout << "[" << channel->getChannelName() << "] connection timeout" << std::endl;
   }

   ClientFactory::stop();

   return allOK ? 0 : 1;
}

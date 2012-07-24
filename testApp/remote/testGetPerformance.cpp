#include <iostream>
#include <pv/clientFactory.h>
#include <pv/pvAccess.h>

#include <stdio.h>
#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsThread.h>
#include <pv/logger.h>
#include <pv/lock.h>

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

#define COUNT 1000   // repetitions per result
#define CHANNELS 1000
#define ARRAY_SIZE 1

#define DEFAULT_TIMEOUT 600.0
#define DEFAULT_REQUEST "field(value)"

double timeOut = DEFAULT_TIMEOUT;
string request(DEFAULT_REQUEST);

PVStructure::shared_pointer pvRequest;

class RequesterImpl : public Requester,
     public std::tr1::enable_shared_from_this<RequesterImpl>
{
public:

    virtual String getRequesterName()
    {
        return "RequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }
};


void usage (void)
{
   fprintf (stderr, "\nUsage: testGetPerformance [options] <PV name>...\n\n"
   "  -h: Help: Print this message\n"
   "options:\n"
   "  -r <pv request>:   Request, specifies what fields to return and options, default is '%s'\n"
   "  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n\n"
            , DEFAULT_REQUEST, DEFAULT_TIMEOUT);
}

vector<ChannelGet::shared_pointer> getCs;
int channelCount = 0;
int allCount = 0;

timeval startTime;

void get_all()
{
    ChannelGet::shared_pointer last;
    for (vector<ChannelGet::shared_pointer>::const_iterator i = getCs.begin();
        i != getCs.end();
        i++)
        {
        (*i)->get(false);
        last = *i;
        }
    last->get(true);
}


class ChannelGetRequesterImpl : public ChannelGetRequester
{
   private:
   ChannelGet::shared_pointer m_channelGet;
   PVStructure::shared_pointer m_pvStructure;
   BitSet::shared_pointer m_bitSet;
   Event m_event;
   Event m_connectionEvent;
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
       std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
   }

   virtual void channelGetConnect(const epics::pvData::Status& status,
        ChannelGet::shared_pointer const & channelGet,
        epics::pvData::PVStructure::shared_pointer const & pvStructure, 
        epics::pvData::BitSet::shared_pointer const & bitSet)
   {
       if (status.isSuccess())
       {
           // show warning
           if (!status.isOk())
           {
               std::cout << "[" << m_channelName << "] channel get create: " << status.toString() << std::endl;
           }

           m_channelGet = channelGet;
           m_pvStructure = pvStructure;
           m_bitSet = bitSet;
           
           m_connectionEvent.signal();

/*
           m_count = COUNT;

           gettimeofday(&m_startTime, NULL);
*/   
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
           if (!status.isOk())
           {
               std::cout << "[" << m_channelName << "] channel get: " << status.toString() << std::endl;
           }

    channelCount++;
    if (channelCount == CHANNELS)
    {
        allCount++;
        channelCount = 0;
    }
//printf("channelCount %d\n", channelCount);
    
    if (allCount == COUNT)
    {
        timeval endTime;
        gettimeofday(&endTime, NULL);
    
    
        long seconds, nseconds;
        double duration;
        seconds  = endTime.tv_sec  - startTime.tv_sec;
        nseconds = endTime.tv_usec - startTime.tv_usec;
    
        duration = seconds + nseconds/1000000.0;
    
        printf("%5.6f seconds, %5.3f (x %d = %5.3f) gets/s\n", duration, COUNT/duration, CHANNELS, COUNT*CHANNELS/duration);
    
        allCount = 0;
        gettimeofday(&startTime, NULL);
        
        get_all();

    }
    else if (channelCount == 0)
    {
            
        get_all();
    }



       }
       else
       {
           std::cout << "[" << m_channelName << "] failed to get: " << status.toString() << std::endl;
       }

   }

/*
   void get()
   {
       return m_event.wait(timeOut);
   }
*/
   bool waitUntilConnected(double timeOut)
   {
       return m_connectionEvent.wait(timeOut);
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
       std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
   }

   virtual void channelCreated(const epics::pvData::Status& status,
            Channel::shared_pointer const & channel)
   {
       if (status.isSuccess())
       {
           // show warning
           if (!status.isOk())
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
printf("this does not work... since this impl. requires bulk get control... tODO\n");
return -1;
   int opt;                    // getopt() current option

   Requester::shared_pointer requester(new RequesterImpl());

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

    printf("%d channels of double array size of %d elements, %d repetitions per sample\n", CHANNELS, ARRAY_SIZE, COUNT);
    
    vector<string> pvs;
    char buf[64];
    for (int i = 0; i < CHANNELS; i++)
    {
        sprintf(buf, "array%d_%d", ARRAY_SIZE, i);
        pvs.push_back(buf);
        //printf("%s\n", buf);
    }


    pvRequest = getCreateRequest()->createRequest(request,requester);
    if(pvRequest.get()==NULL) {
        printf("failed to parse request string\n");
        return 1;
    }

   // typedef enum {logLevelInfo, logLevelDebug, logLevelError, errlogFatal} errlogSevEnum;
   SET_LOG_LEVEL(logLevelError);

   bool allOK = 1;

   ClientFactory::start();
   ChannelProvider::shared_pointer provider = getChannelAccess()->getProvider("pvAccess");


      vector<Channel::shared_pointer> chs;

  for (vector<string>::iterator i = pvs.begin();
        i != pvs.end();
        i++)
        {


   shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new
        ChannelRequesterImpl());
   Channel::shared_pointer channel = provider->createChannel(*i, channelRequesterImpl);
        chs.push_back(channel);
        }

  for (vector<Channel::shared_pointer>::iterator i = chs.begin();
        i != chs.end();
        i++)
        {
          Channel::shared_pointer channel = *i;  
          shared_ptr<ChannelRequesterImpl> channelRequesterImpl = dynamic_pointer_cast<ChannelRequesterImpl>(channel->getChannelRequester());
   if (channelRequesterImpl->waitUntilConnected(5.0))
   {
       shared_ptr<ChannelGetRequesterImpl> getRequesterImpl(new
            ChannelGetRequesterImpl(channel->getChannelName()));
       ChannelGet::shared_pointer channelGet = channel->createChannelGet(getRequesterImpl, pvRequest);

       allOK = getRequesterImpl->waitUntilConnected(timeOut);

       if (!allOK)
       {
           std::cout << "[" << channel->getChannelName() << "] failed to get all the gets" << std::endl;
           return 1;
       }

        getCs.push_back(channelGet);
        
   }
   else
   {
       std::cout << "[" << channel->getChannelName() << "] connection timeout" << std::endl;
       return 1;
   }
}
           std::cout << "all connected" << std::endl;

    gettimeofday(&startTime, NULL);
   get_all();
   
   epicsThreadSleep(DEFAULT_TIMEOUT);
   //ClientFactory::stop();

   return allOK ? 0 : 1;
}

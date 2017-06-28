
/* testChannelConnect.cpp */
/* Author:  Matej Sekoranja Date: 2011.8.24 */


#include <iostream>
#include <sstream>
#include <vector>
#include <stdio.h>
#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsExit.h>
#include <pv/clientContextImpl.h>
#include <pv/clientFactory.h>

#include <pv/event.h>

using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace std;

#define N_CHANNELS_DEFAULT 1000
#define N_RUNS_DEFAULT 1

class ChannelRequesterImpl : public ChannelRequester
{
public:
    ChannelRequesterImpl(size_t channels, Event& event) : total(channels), count(0), g_event(event) {}
private:
    size_t total;
    int count;
    Event& g_event;

    virtual string getRequesterName()
    {
        return "ChannelRequesterImpl";
    };

    virtual void message(std::string const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelCreated(const epics::pvData::Status& status, Channel::shared_pointer const & channel)
    {
        if (!status.isSuccess())
        {
            std::cout << "channelCreated(" << status << ", "
                      << (channel ? channel->getChannelName() : "(0)") << ")" << std::endl;
        }
    }

    // always called from the same thread
    virtual void channelStateChange(Channel::shared_pointer const & c, Channel::ConnectionState connectionState)
    {
        if (connectionState == Channel::CONNECTED)
        {
            cout << c->getChannelName() << " CONNECTED: " << (count+1) << endl;
            if (static_cast<size_t>(++count) == total)
                g_event.signal();
        }
        else if (connectionState == Channel::DISCONNECTED)
        {
            --count;
            cout << c->getChannelName() << " DISCONNECTED: " << count << endl;
        }
        else
            cout << c->getChannelName() << " " << Channel::ConnectionStateNames[connectionState] << endl;

    }
};

void usage (void)
{
    fprintf (stderr, "\nUsage: testChannelConnect [options]\n\n"
             "  -h: Help: Print this message\n"
             "options:\n"
             "  -i <iterations>:   number of iterations per each run (< 0 means forever), default is '%d'\n"
             "  -c <channels>:     number of channels, default is '%d'\n\n"
             , N_RUNS_DEFAULT, N_CHANNELS_DEFAULT);
}


int main (int argc, char *argv[])
{
    size_t nChannels = N_CHANNELS_DEFAULT;
    int runs = N_RUNS_DEFAULT;

    int opt;                    // getopt() current option
    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    // Set stdout to line buffering

    while ((opt = getopt(argc, argv, ":hr:w:i:c:s:l:bf:v")) != -1) {
        switch (opt) {
        case 'h':               // Print usage
            usage();
            return 0;
        case 'i':               // iterations
            runs = atoi(optarg);
            break;
        case 'c':               // channels
        {
            int tmp = atoi(optarg);
            if (tmp < 0) tmp = 1;
            // note: possible overflow (size_t is not always unsigned int)
            nChannels = static_cast<size_t>(tmp);
            break;
        }
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('testChannelConnect -h' for help.)\n",
                    optopt);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('testChannelConnect -h' for help.)\n",
                    optopt);
            return 1;
        default :
            usage();
            return 1;
        }
    }

    {
        Event g_event;

        ClientFactory::start();
        ChannelProvider::shared_pointer provider = ChannelProviderRegistry::clients()->getProvider("pva");

        int run = 0;

        while (runs < 0 || run++ < runs)
        {
            vector<Channel::shared_pointer> channels(nChannels);

            ChannelRequester::shared_pointer channelRequester(new ChannelRequesterImpl(nChannels, g_event));

            char buf[16];
            for (size_t i = 0; i < nChannels; i++)
            {
                sprintf(buf, "test%zu", (i+1));
                channels.push_back(provider->createChannel(buf, channelRequester));
            }

            g_event.wait();

            cout << "connected to all" << endl;
        }

        ClientFactory::stop();
    }

    epicsThreadSleep ( 2.0 );
    //std::cout << "-----------------------------------------------------------------------" << std::endl;
    //epicsExitCallAtExits();
    return(0);
}

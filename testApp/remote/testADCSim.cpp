#ifndef DBVALUE_H
#define DBVALUE_H

#include <epicsTypes.h>
#include <epicsTime.h>

#include <string>
#include <vector>
#include <typeinfo>
#include <pv/sharedPtr.h>

struct baseValue {
    bool remoteWritable;

    typedef std::vector<size_t> shape_t;
    shape_t shape;

    std::string format;

    unsigned short severity;
    std::string message;

    struct timespec timeStamp;

    // static meta-data

    std::string egu;
    int precision;

    baseValue();
};

template<typename PVT>
struct scalarNumericValue : public baseValue {
    typedef PVT value_type;
    typedef PVT element_type;

    value_type value;

    value_type displayHigh, displayLow;
    value_type warnHigh, warnLow;
    value_type alarmHigh, alarmLow;

    value_type ctrlHigh, ctrlLow;
    value_type step;

    scalarNumericValue();
};

struct scalarEnumValue : public  baseValue {
    typedef epicsUInt32 value_type;
    typedef epicsUInt32 element_type;

    value_type value;

    std::vector<std::string> choices;

    scalarEnumValue() : value(0) {}
};

struct scalarStringValue : public  baseValue {
    typedef std::string value_type;
    typedef std::string element_type;

    value_type value;

    std::vector<std::string> choices;
};

template<typename PVT>
struct vectorNumericValue : public baseValue {
    typedef std::tr1::shared_ptr<PVT> value_type;
    typedef PVT element_type;

    value_type value;

    element_type displayHigh, displayLow;

    vectorNumericValue() : displayHigh(0), displayLow(0) {}
};



template<typename PVT>
scalarNumericValue<PVT>::scalarNumericValue()
    :baseValue()
    ,value(0)
    ,displayHigh(0)
    ,displayLow(0)
    ,warnHigh(0)
    ,warnLow(0)
    ,alarmHigh(0)
    ,alarmLow(0)
    ,ctrlHigh(0)
    ,ctrlLow(0)
    ,step(1)
{}


#endif // DBVALUE_H



baseValue::baseValue()
    :remoteWritable(true)
    ,severity(0)
    ,message()
    ,precision(0)
{
    timeStamp.tv_sec = timeStamp.tv_nsec = 0;
}


#ifndef SIM_H
#define SIM_H

//#include <tr1/functional>

#include <epicsThread.h>
#include <epicsEvent.h>

struct SimADC : public std::tr1::enable_shared_from_this<SimADC>,
    public epicsThreadRunable
{
    typedef std::tr1::shared_ptr<SimADC> smart_pointer_type;

    epicsMutex mutex;

    scalarNumericValue<double> mult, shift, offset, freq, rate;

    scalarNumericValue<epicsUInt32> nSamples;
    epicsUInt32 prev_nSamples;

    scalarEnumValue operation;

    vectorNumericValue<double> data;

    vectorNumericValue<epicsUInt32> X;

    epicsThread runner;
    bool runner_stop;
    epicsEvent updated;

    SimADC();
    virtual ~SimADC();

    virtual void run ();
    void cycle();
};

SimADC::smart_pointer_type createSimADC(const std::string& name);
void shutdownSimADCs();

SimADC::smart_pointer_type getSimADC(const std::string& name);


#endif // SIM_H



#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <map>
#include <stdexcept>

#include <epicsGuard.h>
#include <epicsMutex.h>
#include <epicsThread.h>


// 180 / PI
#define RAD2DEG 57.2957795131

// 100 / (2*PI)
#define RAD2PCT 15.9154943092

struct sim_global_type {
    typedef epicsGuard<epicsMutex> guard_t;
    typedef epicsGuardRelease<epicsMutex> unguard_t;
    epicsMutex lock;
    typedef std::map<std::string, SimADC::smart_pointer_type> sims_t;
    sims_t sims;
};

static
sim_global_type *sim_global = 0;

static
void sim_global_init(void*)
{
    try {
        sim_global = new sim_global_type;
    } catch(...) {
        abort();
    }
}

static
epicsThreadOnceId sim_mute_once = EPICS_THREAD_ONCE_INIT;


SimADC::SimADC()
    :runner(*this, "Runner",
            epicsThreadGetStackSize(epicsThreadStackBig),
            epicsThreadPriorityMedium)
    ,runner_stop(false)
{
    mult.value= rate.value= 1.0;
    shift.value= offset.value= 0.0;
    freq.value=90.0;

    offset.egu = mult.egu = "V";
    rate.egu = "Hz";
    rate.displayLow = rate.ctrlLow = 0.0;
    rate.displayHigh = rate.ctrlHigh = 1e6;

    shift.displayLow = shift.ctrlLow = 0.0;
    shift.displayHigh = shift.ctrlHigh = 99.999999;
    shift.egu = "%";

    freq.egu = "deg/pt";


    nSamples.value = 10;
    prev_nSamples = 0;
    cycle();

    operation.choices.resize(2);
    operation.choices[0] = "Stop";
    operation.choices[1] = "Run";
    operation.value = 0;

    data.remoteWritable = false;
    X.remoteWritable = false;

    runner.start();
}

SimADC::~SimADC()
{
    {
        sim_global_type::guard_t G(mutex);
        runner_stop = true;
    }
    runner.exitWait();
}

namespace {
template<typename T>
struct Freeme {
    void operator()(T* p) {
        free(p);
    }
};
}

void SimADC::cycle()
{
    epicsTime now = epicsTime::getCurrent();

    if(nSamples.value != prev_nSamples) {
        X.value.reset();
        data.value.reset();
        prev_nSamples = nSamples.value;
    }

    if(!X.value) {
        X.value.reset((unsigned int*)malloc(sizeof(epicsUInt32)*prev_nSamples), Freeme<unsigned int>());
        unsigned int *val = X.value.get();
        for(size_t i=0; i<prev_nSamples; i++)
            val[i] = 2*i;

        X.shape.resize(1);
        X.shape[0] = prev_nSamples;

        X.displayLow = val[0];
        X.displayHigh = val[prev_nSamples-1];
    }

    if(!data.value || !data.value.unique())
        data.value.reset((double*)malloc(sizeof(double)*prev_nSamples), Freeme<double>());

    X.timeStamp = now;

    if(!X.value) {
        X.severity = 3;
        X.message = "Alloc fails";
    }
    if(!data.value) {
        data.severity = 3;
        data.message = "Alloc fails";
        return;
    }

    double *val = data.value.get();
    for(size_t i=0; i<prev_nSamples; i++)
        val[i] = mult.value * sin((freq.value/RAD2DEG)*i + (shift.value/RAD2PCT)) + offset.value;

    data.shape.resize(1);
    data.shape[0] = prev_nSamples;

    updated.signal();
}

void SimADC::run()
{
    const double min_sleep = epicsThreadSleepQuantum();

    sim_global_type::guard_t G(mutex);

    while(true) {
        {
            double zzz = rate.value>0.0 ? 1.0/rate.value : min_sleep;
            sim_global_type::unguard_t U(G);
            epicsThreadSleep(zzz);
        }
        if(runner_stop)
            break;

        cycle();
    }
    //printf("SimADC shutdown\n");
}

SimADC::smart_pointer_type createSimADC(const std::string& name)
{
    epicsThreadOnce(&sim_mute_once, &sim_global_init, 0);

    sim_global_type::guard_t G(sim_global->lock);

    sim_global_type::sims_t &sims = sim_global->sims;

    SimADC::smart_pointer_type P(new SimADC);

    sims[name] = P;

    return P;
}

void shutdownSimADCs()
{
    assert(sim_global);

    sim_global_type::sims_t sims;

    {
        /* swap out so the global lock is not held during delete */
        sim_global_type::guard_t G(sim_global->lock);
        sims.swap(sim_global->sims);
    }

    sims.clear();
}

SimADC::smart_pointer_type getSimADC(const std::string& name)
{
    epicsThreadOnce(&sim_mute_once, &sim_global_init, 0);

    sim_global_type::guard_t G(sim_global->lock);

    sim_global_type::sims_t::const_iterator it = sim_global->sims.find(name);

    if(it==sim_global->sims.end())
        return SimADC::smart_pointer_type();

    return it->second;
}


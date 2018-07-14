# pvAccessCPP: ca provider

2018.07.09

Editors:

* Marty Kraimer

This is a description of channel provider **ca** that is implemented as part of **pvAccessCPP**.

It uses the **channel access** network protocol to communicate with a server,
i. e. the network protocol that has been used to communicate with **EPICS IOCs** since 1990.

Provider **pva** is another way to connect to a **DBRecord**,
But this only works if the IOC has **qsrv** installed.
**qsrv**, which is provided with
[pva2pva](https://github.com/epics-base/pva2pva),
has full support for communicating with a **DBRecord**.
The only advantage of **ca** is that it does require any changes to an existing IOC.


The following are discussed.

* [Introduction](#S-introduction)
* [client API](#S-client)
* [Mapping DBD data to pvData](#S-dbd_to_pvdata)
* [Developing plugins for ca provider.](#S-plugin)



## <a name="S-introduction"></a>Introduction

The primary purpose of the **ca** provider is to access **DBRecord**s in an EPICS IOC via the
**channel access** network protocol but to use pvData objects for the client.

Each **DBRecord** instance has a record name that must be unique in the local area network.
A client can access any public field of a **DBRecord**;
Each **DBRecord** instance has a record name that is unique with in the local area network
A channel name is a **recordname.fieldName**.
If the fieldname is not specified then **.VAL** is assumed

### example database

The following:

```
mrk> pwd
/home/epicsv4/masterCPP/pvAccessCPP/testCa
mrk> softIoc -d testCaProvider.db
```

Starts an EPICS IOC that is used for all examples in this document.

### examples

```
mrk> caget -d DBR_TIME_FLOAT DBRdoubleout
DBRdoubleout
    Native data type: DBF_DOUBLE
    Request type:     DBR_TIME_FLOAT
    Element count:    1
    Value:            1
    Timestamp:        2018-06-21 06:23:07.939894
    Status:           NO_ALARM
    Severity:         NO_ALARM
mrk> pvget -p ca -r "value,alarm,timeStamp" -i DBRdoubleout
DBRdoubleout
structure 
    double value 1
    alarm_t alarm
        int severity 0
        int status 0
        string message 
    time_t timeStamp
        long secondsPastEpoch 1529576587
        int nanoseconds 939894210
        int userTag 0
mrk> pvget -p ca -r "value,alarm,timeStamp" DBRdoubleout
DBRdoubleout
structure 
    double value 1
    alarm_t alarm NO_ALARM NO_STATUS <no message>
    time_t timeStamp 2018-06-21T06:23:07.940 0 

mrk> pvget -p ca -r value -i DBRdoubleout.SCAN
DBRdoubleout.SCAN
epics:nt/NTEnum:1.0 
    enum_t value
        int index 0
        string[] choices [Passive,Event,I/O Intr,10 second,5 second,2 second,1 second,.5 second,.2 second,.1 second]
```

### Overview of Channel Access 

**channel access**, which is provided with **epics-base**, defines and implements a protocol
for client/server network communication.

**epics-base** provides both a client and a server implementation
This document only discusses the client API.

For details see:

[EPICS Channel Access 4.13.1 Reference Manual](https://epics.anl.gov/base/R7-0/1-docs/CAref.html)

**channel access** allows a client to get, put, and monitor monitor data from a server.
The data is defined by various DBD types.

The following, in **epics-base/include**, are the
main include files that show the **channel access** API:

```
cadef.h
db_access.h
```

The client requests data via one of the DBR types.

For example:

```
DBR_STS_DOUBLE	returns a double status structure (dbr_sts_double)
where
struct dbr_sts_double{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_long_t	RISC_pad;		/* RISC alignment */
	dbr_double_t	value;			/* current value */
};
```

The server converts data between the native type of the field being accessed and the DBR type.


### Overview of ca provider

**ca** is a pvAccess Channel Provider that uses **channel access** to connect a client to a server.

With **ca**, the client data appears as pvData objects, i. e.
**ca** converts the data provided by **channel access** to/from pvData

Thus a pvAccess client can communicate with an existing V3 EPICS IOC without making
any changes to existing IOCs.

For an overview of pvData and pvAccess see:

[EPICS V4 Developer's Guide](https://mrkraimer.github.io/website/developerGuide/developerGuide.html)

**ca** requests data from the server with a DBR type that matches the native type.
See the next section for more details.

All conversion to/from other types must be done by the client.

## <a name="S-client"></a>Client API

**ca** implements the following pvAccess methods : **getField**, **channelGet**, **channelPut** and **monitor**.

For channelPut the only field that can be accessed is **value**.
For channelPut a client can issue puts with and without a callback from the server.
The default is no callback. If createChannelPut has the option "record[block=true]" then a put callback used.

All of the other pvAccess methods provide access to fields **alarm** and **timeStamp**.

Depending on the type associated with the **value** field the following fields may also be available:
**display**, **control** , and **valueAlarm**.

Thus a client can make requests like:

```
pvget -p ca -r "value,alarm,timeStamp,display,control,valueAlarm" names ...
```

**ca** will create a structure that has the fields requested but only for fields that are supported
by the server.

* For puts only value is supported.
* For gets and monitors every channel supports value, alarm, and timeStamp;
* If any of display,control, or valueAlarm are requested then timeStamp is NOT available.

Lets discuss the various fields.

### value

This can be a scalar, scalarArray, or an enumerated structure.

For a scalar or scalarArray the ScalarType is one of the following:
**pvString**, **pvByte**, **pvShort**, **pvInt**, **pvFloat**, or **pvDouble**.

Note that **channel access** does not support unsigned integers or 64 bit integers.

A enumerated structure is created if the native type is **DBR_ENUM**.

Some examples are:

```
pvget -p ca -r value -i DBRlongout
DBRlongout
structure 
    int value 0
mrk> pvget -p ca -r value -i DBRdoubleout
DBRdoubleout
structure 
    double value 0
mrk> pvget -p ca -r value -i DBRshortArray
DBRshortArray
structure 
    short[] value []
mrk> pvget -p ca -r value -i DBRstringArray
DBRstringArray
structure 
    string[] value [aa,bb,cc]
mrk> pvget -p ca -r value -i DBRmbbin
DBRmbbin
epics:nt/NTEnum:1.0 
    enum_t value
        int index 1
        string[] choices [zero,one,two,three,four,five,six,seven,eight,nine,ten,eleven,twelve,thirteen,fourteen,fifteen]
mrk> 

```


### alarm,timeStamp,display,control, and valueAlarm

Each of these is one of the property structures defined in pvData.

#### Examples

```
mrk> pvget -p ca -r alarm -i DBRdoubleout
DBRdoubleout
structure 
    alarm_t alarm
        int severity 2
        int status 3
        string message HIHI

mrk> pvget -p ca -r timeStamp -i DBRdoubleout
DBRdoubleout
structure 
    time_t timeStamp
        long secondsPastEpoch 1529923341
        int nanoseconds 314916189
        int userTag 0
mrk> pvget -p ca -r display -i DBRdoubleout
DBRdoubleout
structure 
    display_t display
        double limitLow -10
        double limitHigh 10
        string description 
        string format F8.2
        string units volts
mrk> pvget -p ca -r control -i DBRdoubleout
DBRdoubleout
structure 
    control_t control
        double limitLow -1e+29
        double limitHigh 1e+29
        double minStep 0
mrk> pvget -p ca -r valueAlarm -i DBRdoubleout
DBRdoubleout
structure 
    valueAlarm_t valueAlarm
        boolean active false
        double lowAlarmLimit -8
        double lowWarningLimit -6
        double highWarningLimit 6
        double highAlarmLimit 8
        int lowAlarmSeverity 0
        int lowWarningSeverity 0
        int highWarningSeverity 0
        int highAlarmSeverity 0
```



## <a name="S-dbd_to_pvdata"></a>DBD to pvData

### Type Conversion

Three type systems are involved in accessing data in a **DBRecord** and converting it to/from pvData:

* DBF The type system used for **DBRecord**s.
* DBR The type system used by **channel access**.
* pvData

The following gives a summary of the conversions between the type systems:

```
rawtype               DBF          DBR         pvData ScalarType

char[MAX_STRING_SIZE] DBF_STRING   DBR_STRING  pvString
epicsInt8             DBF_CHAR     DBR_CHAR    pvByte
epicsUint8            DBF_UCHAR    DBR_CHAR    pvByte
epicsInt16            DBF_SHORT    DBR_SHORT   pvShort
epicsUInt16           DBF_USHORT   DBR_LONG    pvInt
epicsInt32            DBF_LONG     DBR_LONG    pvInt
epicsUInt32           DBF_ULONG    DBR_DOUBLE  pvDouble
epicsInt64            DBF_INT64    no support
epicsUInt64           DBF_UINT64   no support
float                 DBF_FLOAT    DBR_FLOAT   pvFloat
double                DBF_DOUBLE   DBR_DOUBLE  pvDouble
epicsUInt16           DBF_ENUM     DBR_ENUM    enum structure
epicsUInt16           DBF_MENU     DBR_ENUM    enum structure
```

Notes:

* Both DBF_CHAR and DBF_UCHAR go to DBR_CHAR. This is ambigous.
* DBF_USHORT promoted to DBR_LONG
* DBF_ULONG promoted to DBR_DOUBLE 
* qsrv provides full access to all DBF types, but the IOC must have qsrv installed. 

### Accessing data in a DBRecord

An IOC database is a memory resident database of **DBRecord** instances.

Each **DBRecord** is an instance of one of an extensible set of record types.
Each record type has an associated dbd definition which defines a set of fields for
each record instance.

For example an aoRecord.dbd has the definition:

```
recordtype(ao) {
    include "dbCommon.dbd" 
    field(VAL,DBF_DOUBLE) {
        ...
    }
    field(OVAL,DBF_DOUBLE) {
        ...
    }
    ... many more fields
```

In addition each record type has a associated set of support code defined in recSup.h

```
/* record support entry table */
struct typed_rset {
    long number; /* number of support routines */
    long (*report)(void *precord);
    long (*init)();
    long (*init_record)(struct dbCommon *precord, int pass);
    long (*process)(struct dbCommon *precord);
    long (*special)(struct dbAddr *paddr, int after);
    long (*get_value)(void); /* DEPRECATED set to NULL */
    long (*cvt_dbaddr)(struct dbAddr *paddr);
    long (*get_array_info)(struct dbAddr *paddr, long *no_elements, long *offset);
    long (*put_array_info)(struct dbAddr *paddr, long nNew);
    long (*get_units)(struct dbAddr *paddr, char *units);
    long (*get_precision)(const struct dbAddr *paddr, long *precision);
    long (*get_enum_str)(const struct dbAddr *paddr, char *pbuffer);
    long (*get_enum_strs)(const struct dbAddr *paddr, struct dbr_enumStrs *p);
    long (*put_enum_str)(const struct dbAddr *paddr, const char *pbuffer);
    long (*get_graphic_double)(struct dbAddr *paddr, struct dbr_grDouble *p);
    long (*get_control_double)(struct dbAddr *paddr, struct dbr_ctrlDouble *p);
    long (*get_alarm_double)(struct dbAddr *paddr, struct dbr_alDouble *p);
};
```

The methods that support accessing data from the record include:

```
cvt_dbaddr          Implemented by record types that determine VAL type at record initialization
*array_info         Implemented by array record types
get_units           Implemented by numeric record types
get_precision       Implemented by float and double record types
*_enum_*            Implemented by enumerated record types
get_graphic_double  NOTE Always returns limits as double
get_control_double  NOTE Always returns limits as double 
get_alarm_double    NOTE Always returns limits as double
```

Each of these methods is optional, i. e. record support for a particular record type
only implements methods that make sense for the record type.

For example the enum methods are only implemented by records that have the definition:

```
...
    field(VAL,DBF_ENUM) {
...
}
...
```
 

### Channel Access Data

A client can access any public field of a **DBRecord**;
Each **DBRecord** instance has a record name that is unique within the local area network.

A channel name is a **recordname.fieldName**.

If the fieldname is not specified then **.VAL** is assumed and the record support methods shown
above can also be used to get additional data from the record.

Any field that is accessable by client code must have a vald DBF_ type.

A client gets/puts data via a **DBR_*** request.

The basic DBR types are:
```
rawtype               DBR

char[MAX_STRING_SIZE] DBR_STRING
epicsInt8             DBR_CHAR
epicsInt16            DBR_SHORT
epicsInt32            DBR_LONG
float                 DBF_FLOAT
double                DBF_DOUBLE
epicsUInt16           DBR_ENUM
```

In addition to the DBR basic types the following DBR types provide additional data:

```
DBR             one of the types above.
DBR_STATUS_*    adds status and severity to DBR.
DBR_TIME_*      adds epicsTimeStamp to DBR_STATUS.
DBR_GR_*        adds display limits to DBR_STATUS. NOTE: no epicsTimeStamp
DBR_CTRL_       adds control limits to DBR_GR.     NOTE: no epicsTimeStamp
DBR_CTRL_ENUM   This is a special case.
```

NOTES:

* status, severity, and epicsTimeStamp are the same for each DBR type.
* limits have the same types as the correspondng DBR type.
* server converts limits from double to the DBR type.
* GR and CTRL have precision only for DBR_FLOAT and DBR_DOUBLE


Some examples:

```
DBR_STS_DOUBLE	returns a double status structure (dbr_sts_double)
where
struct dbr_sts_double{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_long_t	RISC_pad;		/* RISC alignment */
	dbr_double_t	value;			/* current value */
};

DBR_TIME_DOUBLE	returns a double time structure (dbr_time_double)
where
struct dbr_time_double{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	epicsTimeStamp	stamp;			/* time stamp */
	dbr_long_t	RISC_pad;		/* RISC alignment */
	dbr_double_t	value;			/* current value */
};

DBR_GR_SHORT	returns a graphic short structure (dbr_gr_short)
where
struct dbr_gr_short{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	char		units[MAX_UNITS_SIZE];	/* units of value */
	dbr_short_t	upper_disp_limit;	/* upper limit of graph */
	dbr_short_t	lower_disp_limit;	/* lower limit of graph */
	dbr_short_t	upper_alarm_limit;	
	dbr_short_t	upper_warning_limit;
	dbr_short_t	lower_warning_limit;
	dbr_short_t	lower_alarm_limit;
	dbr_short_t	value;			/* current value */
};

DBR_GR_DOUBLE	returns a graphic double structure (dbr_gr_double)
where
struct dbr_gr_double{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_short_t	precision;		/* number of decimal places */
	dbr_short_t	RISC_pad0;		/* RISC alignment */
	char		units[MAX_UNITS_SIZE];	/* units of value */
	dbr_double_t	upper_disp_limit;	/* upper limit of graph */
	dbr_double_t	lower_disp_limit;	/* lower limit of graph */
	dbr_double_t	upper_alarm_limit;	
	dbr_double_t	upper_warning_limit;
	dbr_double_t	lower_warning_limit;
	dbr_double_t	lower_alarm_limit;
	dbr_double_t	value;			/* current value */
};

DBR_CTRL_DOUBLE	returns a control double structure (dbr_ctrl_double)
where
struct dbr_ctrl_double{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_short_t	precision;		/* number of decimal places */
	dbr_short_t	RISC_pad0;		/* RISC alignment */
	char		units[MAX_UNITS_SIZE];	/* units of value */
	dbr_double_t	upper_disp_limit;	/* upper limit of graph */
	dbr_double_t	lower_disp_limit;	/* lower limit of graph */
	dbr_double_t	upper_alarm_limit;	
	dbr_double_t	upper_warning_limit;
	dbr_double_t	lower_warning_limit;
	dbr_double_t	lower_alarm_limit;
	dbr_double_t	upper_ctrl_limit;	/* upper control limit */
	dbr_double_t	lower_ctrl_limit;	/* lower control limit */
	dbr_double_t	value;			/* current value */
};


DBR_CTRL_ENUM	returns a control enum structure (dbr_ctrl_enum)
where
struct dbr_ctrl_enum{
	dbr_short_t	status;	 		/* status of value */
	dbr_short_t	severity;		/* severity of alarm */
	dbr_short_t	no_str;			/* number of strings */
	char	strs[MAX_ENUM_STATES][MAX_ENUM_STRING_SIZE];
					/* state strings */
	dbr_enum_t	value;		/* current value */
};
```

### PVData for a DBRrecord via ca provider

**pvAccessCPP/src/ca** has files **dbdToPv.h** and **dbdToPv.cpp**.
This is the code that converts between DBD data and pvData.

This code must decide which of the many **DBR_*** types to use.


There is a static method:

```
static DbdToPvPtr create(
    CAChannelPtr const & caChannel,
    epics::pvData::PVStructurePtr const & pvRequest,
    IOType ioType);  // one of getIO, putIO, and monitorIO
```


When this is called the first thing is to determine which fields are requested by the client.
This is from the set **value**, **alarm**, **timeStamp**. **display**, **control** , and **valueAlarm**.


* If the ioType is putIO only **value** is valid.
* If the channel type is **DBR_ENUM** then **display**, **control** , and **valueAlarm** are ignored.
* If the channel is an array then **control** , and **valueAlarm** are ignored.
* If the channel type is **DBR_STRING** then **display**, **control** , and **valueAlarm** are ignored.
* If any of **display**, **control** , and **valueAlarm** are still allowed then **timeStamp** is ignored,
because the DBR type selected will not return the timeStamp.

If ths channel type is **DBR_ENUM** a one time **ca_array_get_callback(DBR_GR_ENUM...** request is issued
to get the choices for the enumerated value.

Depending or which fields are still valid, the  DBR type is obtained via

* If any of **display**, **control** ,or **valueAlarm** is valid then **dbf_type_to_DBR_CTRL(caValueType)** .
* else If **alarm** or **timeStamp** is valid then **dbf_type_to_DBR_TIME(caValueType)** .
* else **dbf_type_to_DBR(caValueType)**

Where **caValueType** is one of DBR_STRING, DBR_SHORT, DBR_FLOAT, DBR_ENUM, DBR_CHAR, DBR_LONG, DBR_DOUBLE.

If **display** is still valid then the following call is made:

```
string name(caChannel->getChannelName() + ".DESC");
int result = ca_create_channel(name.c_str(),
...
```
When the channel connects a get is issued to get the value for **display.description**.
 
## <a name="S-plugin"></a>Developing plugins for ca provider

This section provides guidelines for code developers that use **ca**  to connect a client to a server.
This includes plugins for things like MEDM, EDM, caqtDM, etc.
But also means any code that use **ca**: pvget, pvput, pvaClientCPP, exampleCPP/exampleClient, etc.

The **channel access** reference manual describes channel context:

[CA Client Contexts and Application Specific Auxiliary Threads](https://epics.anl.gov/base/R7-0/1-docs/CAref.html#Client2)

A brief summary of channel context is:


* Only the thread that calls CAClientFactory::start() and associated auxillary threads
can call **ca_xxx** functions.

The public access to **ca** is:

```
class epicsShareClass CAClientFactory
{
public:
    /** @brief start provider ca
     *
     */
    static void start();
    /** @brief get the ca_client_context
     *
     * This can be called by an application specific auxiliary thread.
     * See ca documentation. Not for casual use.
     */
    static ca_client_context * get_ca_client_context();
    /** @brief stop provider ca
     *
     * This does nothing since epicsAtExit is used to destroy the instance.
     */
    static void stop();
};
```
Any code that uses **ca** must call **CAClientFactory::start()** before making any pvAccess client requests.

ca_context_create is called for the thread that calls CAClientFactory::start().

If the client creates auxillary threads the make pvAccess client requests then the auxillary threads will automatically become
a **ca** auxilary thread.


[Deadlock in ca_clear_subscription()](https://bugs.launchpad.net/epics-base/7.0/+bug/1751380)

Shows a problem with monitor callbacks.
A test was created that shows that the same problem can occur with a combination of rapid get, put and monitor events.


In order to prevent this problem **ca** creates the following threads:
**getEventThread**, **putEventThread**, and **monitorEventThread**.

All client callbacks are made via one of these threads.
For example a call to the requester's **monitorEvent** method is made from the monitorEventThread.

**Notes**

* These threads do not call **ca_attach_context**.
* No **ca_xxx** function should be called from the requester's callback method.


 


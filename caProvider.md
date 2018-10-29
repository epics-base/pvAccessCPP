# pvAccessCPP: ca provider

2018.10.05

Editors:

* Marty Kraimer

**ca** is a channel provider **ca** that is implemented as part of **pvAccessCPP**.


It uses the **channel access** network protocol to communicate with a server,
i. e. the network protocol that has been used to communicate with **EPICS IOCs** since 1990.

A description of **ca** is provided in
[caProvider](https://mrkraimer.github.io/website/caProvider/caProvider.html)

Provider **pva** is another way to connect to a **DBRecord**,
But this only works if the IOC has **qsrv** installed.
**qsrv**, which is provided with
[pva2pva](https://github.com/epics-base/pva2pva),
has full support for communicating with a **DBRecord**.
The only advantage of **ca** is that it does require any changes to an existing IOC.




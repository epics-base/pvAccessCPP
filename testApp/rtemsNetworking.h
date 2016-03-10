/*
 * Network configuration -- QEMU NOT using DHCP
 *
 ************************************************************
 * EDIT THIS FILE TO REFLECT YOUR NETWORK CONFIGURATION     *
 * BEFORE RUNNING ANY RTEMS PROGRAMS WHICH USE THE NETWORK! *
 ************************************************************
 *
 *  The dynamic probing is based upon the EPICS network
 *  configuration file written by:
 *      W. Eric Norum
 *      eric.norum@usask.ca
 *      (306) 966-5394
 */

#ifndef _RTEMS_NETWORKCONFIG_H_
#define _RTEMS_NETWORKCONFIG_H_

/* #define USE_LIBBSDPORT */

#if defined(USE_LIBBSDPORT)
#include <bsp/libbsdport_api.h>
#define CONFIGURE_MAXIMUM_TIMERS 10
#endif
/*
 * For TFTP test application
 */
#if (defined (RTEMS_USE_BOOTP))
#define RTEMS_TFTP_TEST_HOST_NAME "BOOTP_HOST"
#define RTEMS_TFTP_TEST_FILE_NAME "BOOTP_FILE"
#else
#define RTEMS_TFTP_TEST_HOST_NAME "XXX.YYY.ZZZ.XYZ"
#define RTEMS_TFTP_TEST_FILE_NAME "tftptest"
#endif

/*
 * For NFS test application
 *
 * NFS mount and a directory to ls once mounted
 */
#define RTEMS_NFS_SERVER      "192.168.1.210"
#define RTEMS_NFS_SERVER_PATH "/home"
#define RTEMS_NFS_LS_PATH     "/mnt/nfstest"



/*
 * This file can be copied to an application source dirctory
 * and modified to override the values shown below.
 *
 * The following CPP symbols may be passed from the Makefile:
 *
 *   symbol                   default       description
 *
 *   NETWORK_TASK_PRIORITY    150           can be read by app from public
 *                                          var 'gesysNetworkTaskPriority'
 *   FIXED_IP_ADDR            <undefined>   hardcoded IP address (e.g.,
 *                                          "192.168.0.10"); disables BOOTP;
 *                                          must also define FIXED_NETMASK
 *   FIXED_NETMASK            <undefined>   IP netmask string
 *                                          (e.g. "255.255.255.0")
 *   MULTI_NETDRIVER          <undefined>   ugly hack; if defined try to probe
 *                                          a variety of PCI and ISA drivers
 *                                          (i386 ONLY) use is discouraged!
 *   NIC_NAME                 <undefined>   Ethernet driver name (e.g. "pcn1");
 *                                          must also define NIC_ATTACH
 *   NIC_ATTACH               <undefined>   Ethernet driver attach function
 *                                          (e.g., rtems_fxp_attach).
 *                                          If these are undefined then
 *                                            a) MULTI_NETDRIVER is used
 *                                               (if defined)
 *                                            b) RTEMS_BSP_NETWORK_DRIVER_NAME/
 *                                               RTEMS_BSP_NETWORK_DRIVER_ATTACH
 *                                               are tried
 *   MEMORY_CUSTOM            <undefined>   Allocate the defined amount of
 *                                          memory for mbufs and mbuf clusters,
 *                                          respectively. Define to a comma ','
 *                                          separated pair of two numerical
 *                                          values, e.g: 100*1024,200*1024
 *   MEMORY_SCARCE            <undefined>   Allocate few memory for mbufs
 *                                          (hint for how much memory the
 *                                          board has)
 *   MEMORY_HUGE              <undefined>   Allocate a lot of memory for mbufs
 *                                          (hint for how much memory the
 *                                          board has)
 *                                          If none of MEMORY_CUSTOM/
 *                                          MEMORY_SCARCE/MEMORY_HUGE are
 *                                          defined then a medium amount of
 *                                          memory is allocated for mbufs.
 */

#include <rtems/bspIo.h>
#include <bsp.h>
#include <rtems/rtems_bsdnet.h>

#if 0
#ifdef HAVE_CONFIG_H
#include <config.h>
#else
#include "verscheck.h"
#endif
#endif

//#define MULTI_NETDRIVER
//#define RTEMS_BSP_NETWORK_DRIVER_NAME 1

#define FIXED_IP_ADDR "192.168.1.249"
#define FIXED_NETMASK "255.255.255.0"

#ifndef NETWORK_TASK_PRIORITY
#define NETWORK_TASK_PRIORITY   150  /* within EPICS' range */
#endif

/* make publicily available for startup scripts... */
const int gesysNetworkTaskPriority = NETWORK_TASK_PRIORITY;

#ifdef  FIXED_IP_ADDR
#define RTEMS_DO_BOOTP 0
#else
#define RTEMS_DO_BOOTP rtems_bsdnet_do_bootp
#define FIXED_IP_ADDR  0
#undef  FIXED_NETMASK
#define FIXED_NETMASK  0
#endif

#if !defined(NIC_NAME)

#ifdef MULTI_NETDRIVER

#if 0
#if RTEMS_VERSION_ATLEAST(4,6,99)
#define pcib_init pci_initialize
#endif
#endif

extern int rtems_3c509_driver_attach (struct rtems_bsdnet_ifconfig *, int);
extern int rtems_fxp_attach (struct rtems_bsdnet_ifconfig *, int);
extern int rtems_elnk_driver_attach (struct rtems_bsdnet_ifconfig *, int);
extern int rtems_dec21140_driver_attach (struct rtems_bsdnet_ifconfig *, int);

/* these don't probe and will be used even if there's no device :-( */
extern int rtems_ne_driver_attach (struct rtems_bsdnet_ifconfig *, int);
extern int rtems_wd_driver_attach (struct rtems_bsdnet_ifconfig *, int);

static struct rtems_bsdnet_ifconfig isa_netdriver_config[] = {
    {
        "ep0", rtems_3c509_driver_attach, isa_netdriver_config + 1,
    },
    {
        "ne1", rtems_ne_driver_attach, 0, irno: 9 /* qemu cannot configure irq-no :-(; has it hardwired to 9 */
    },
};

static struct rtems_bsdnet_ifconfig pci_netdriver_config[]= {
    {
        "dc1", rtems_dec21140_driver_attach, pci_netdriver_config+1,
    },
#if !defined(USE_LIBBSDPORT)
    {
        "fxp1", rtems_fxp_attach, pci_netdriver_config+2,
    },
#else
    {
        "", libbsdport_netdriver_attach, pci_netdriver_config+2,
    },
#endif
    {
        "elnk1", rtems_elnk_driver_attach, isa_netdriver_config,
    },
};

static int pci_check(struct rtems_bsdnet_ifconfig *ocfg, int attaching)
{
    struct rtems_bsdnet_ifconfig *cfg;
    int if_index_pre;
    extern int if_index;
    if ( attaching ) {
        cfg = pci_initialize() ?
              isa_netdriver_config : pci_netdriver_config;
    }
    while ( cfg ) {
        printk("Probing '%s'", cfg->name);
        /* unfortunately, the return value is unreliable - some drivers report
         * success even if they fail.
         * Check if they chained an interface (ifnet) structure instead
         */
        if_index_pre = if_index;
        cfg->attach(cfg, attaching);
        if ( if_index > if_index_pre ) {
            /* assume success */
            printk(" .. seemed to work\n");
            ocfg->name   = cfg->name;
            ocfg->attach = cfg->attach;
            return 0;
        }
        printk(" .. failed\n");
        cfg = cfg->next;
    }
    return -1;
}


#define NIC_NAME   "dummy"
#define NIC_ATTACH pci_check

#else

#if defined(RTEMS_BSP_NETWORK_DRIVER_NAME)  /* Use NIC provided by BSP */

/* force ne2k_isa on i386 for qemu */
#if defined(__i386__)
# define NIC_NAME  BSP_NE2000_NETWORK_DRIVER_NAME
# define NIC_ATTACH BSP_NE2000_NETWORK_DRIVER_ATTACH

#else

# define NIC_NAME  RTEMS_BSP_NETWORK_DRIVER_NAME
# define NIC_ATTACH RTEMS_BSP_NETWORK_DRIVER_ATTACH
#endif

#endif

#endif /* ifdef MULTI_NETDRIVER */
#endif


#ifdef NIC_NAME

extern int NIC_ATTACH();

#if RTEMS_BSP_NETWORK_DRIVER_ATTACH == BSP_NE2000_NETWORK_DRIVER_ATTACH
static char ethernet_address[6] = { 0x00, 0xab, 0xcd, 0xef, 0x12, 0x34 };
#endif

static struct rtems_bsdnet_ifconfig netdriver_config[1] = {{
        NIC_NAME,  /* name */
        (int (*)(struct rtems_bsdnet_ifconfig*,int))NIC_ATTACH,  /* attach function */
        0,  		/* link to next interface */
        FIXED_IP_ADDR,
        FIXED_NETMASK
#if RTEMS_BSP_NETWORK_DRIVER_ATTACH == BSP_NE2000_NETWORK_DRIVER_ATTACH
        ,
        ethernet_address,
        irno:9,
        port:0xc100
#endif
    }
};
#else
#warning "NO KNOWN NETWORK DRIVER FOR THIS BSP -- YOU MAY HAVE TO EDIT networkconfig.h"
#endif

struct rtems_bsdnet_config rtems_bsdnet_config = {
#ifdef NIC_NAME
    netdriver_config,         /* link to next interface */
    RTEMS_DO_BOOTP,           /* Use BOOTP to get network configuration */
#else
    0,
    0,
#endif
    NETWORK_TASK_PRIORITY,    /* Network task priority */
#if   defined(MEMORY_CUSTOM)
    MEMORY_CUSTOM,
#elif defined(MEMORY_SCARCE)
    100*1024,                 /* MBUF space */
    200*1024,                 /* MBUF cluster space */
#elif defined(MEMORY_HUGE)
    2*1024*1024,              /* MBUF space */
    5*1024*1024,              /* MBUF cluster space */
#else
    180*1024,                 /* MBUF space */
    350*1024,                 /* MBUF cluster space */
#endif
#if (!defined (RTEMS_USE_BOOTP)) && defined(ON_RTEMS_LAB_WINSYSTEMS)
    "rtems",                /* Host name */
    "nodomain.com",         /* Domain name */
    "192.168.1.14",         /* Gateway */
    "192.168.1.1",          /* Log host */
    {"89.212.75.6"  },      /* Name server(s) */
    {"192.168.1.1"  },      /* NTP server(s) */
#else
    NULL,                   /* Host name */
    NULL,                   /* Domain name */
    NULL,                   /* Gateway */
    NULL,                   /* Log host */
    { NULL },               /* Name server(s) */
    { NULL },               /* NTP server(s) */
#endif /* !RTEMS_USE_BOOTP */
    0,                      /* efficiency */
    0,                      /* udp TX buffer */
    0,                      /* udp RX buffer */
    0,                      /* tcp TX buffer */
    0,                      /* tcp RX buffer */
};
#endif /* _RTEMS_NETWORKCONFIG_H_ */

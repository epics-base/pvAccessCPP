project('epics-pv-access', 'cpp', version: '7.1.6', license: 'EPICS')

epics_com_dep = dependency('epics-com')
epics_pv_data_dep = dependency('epics-pv-data')

pva_version_num = configure_file(
  #input: 'src/pv/pvdVersionNum.h@',
  output: 'pvaVersionNum.h',
  configuration: configuration_data({
    'EPICS_PVA_MAJOR_VERSION': '7',
    'EPICS_PVA_MINOR_VERSION': '1',
    'EPICS_PVA_MAINTENANCE_VERSION': '6',
    'EPICS_PVA_DEVELOPMENT_FLAG': '1',
  }),
)

# HACK: this installs 'pvdVersionNum.h' in a 'pv' subdir
# See: https://github.com/mesonbuild/meson/issues/2320
run_command(
  [
    'sh',
    '-c',
    'mkdir @1@/pv; cp @1@/@0@ @1@/pv/'.format(
      pva_version_num,
      meson.current_build_dir()
    ),
  ],
  check: true,
)

headers = [
  'src/client/clientpvt.h',
  'src/client/pv/monitor.h',
  'src/client/pv/pvAccess.h',
  'src/client/pva/client.h',

  'src/mb/pv/pvAccessMB.h',

  'src/pipelineService/pv/pipelineServer.h',
  'src/pipelineService/pv/pipelineService.h',

  'src/pva/pv/clientFactory.h',
  'src/pva/pv/pvaConstants.h',
  'src/pva/pv/pvaDefs.h',
  'src/pva/pv/pvaVersion.h',
  pva_version_num,

  'src/remote/pv/beaconHandler.h',
  'src/remote/pv/blockingTCP.h',
  'src/remote/pv/blockingUDP.h',
  'src/remote/pv/channelSearchManager.h',
  'src/remote/pv/codec.h',
  'src/remote/pv/remote.h',
  'src/remote/pv/security.h',
  'src/remote/pv/securityImpl.h',
  'src/remote/pv/serializationHelper.h',
  'src/remote/pv/transportRegistry.h',

  'src/remoteClient/pv/clientContextImpl.h',

  'src/rpcClient/pv/rpcClient.h',

  'src/rpcService/pv/rpcServer.h',
  'src/rpcService/pv/rpcService.h',

  'src/server/pv/baseChannelRequester.h',
  'src/server/pv/beaconEmitter.h',
  'src/server/pv/beaconServerStatusProvider.h',
  'src/server/pv/responseHandlers.h',
  'src/server/pv/serverChannelImpl.h',
  'src/server/pv/serverContext.h',
  'src/server/pv/serverContextImpl.h',
  'src/server/pva/server.h',
  'src/server/pva/sharedstate.h',
  'src/server/sharedstateimpl.h',

  'src/utils/pv/configuration.h',
  'src/utils/pv/destroyable.h',
  'src/utils/pv/fairQueue.h',
  'src/utils/pv/hexDump.h',
  'src/utils/pv/inetAddressUtil.h',
  'src/utils/pv/introspectionRegistry.h',
  'src/utils/pv/likely.h',
  'src/utils/pv/logger.h',
  'src/utils/pv/referenceCountingLock.h',
  'src/utils/pv/requester.h',
  'src/utils/pv/wildcard.h',
]

sources = [
  # TODO: once CA is packaged, as separate libpvAccessCA library
  #'src/ca/caChannel.cpp',
  #'src/ca/caContext.cpp',
  #'src/ca/caProvider.cpp',
  #'src/ca/dbdToPv.cpp',
  #'src/ca/notifierConveyor.cpp',

  'src/client/client.cpp',
  'src/client/clientGet.cpp',
  'src/client/clientInfo.cpp',
  'src/client/clientMonitor.cpp',
  'src/client/clientPut.cpp',
  'src/client/clientRPC.cpp',
  'src/client/clientSync.cpp',
  'src/client/monitor.cpp',
  'src/client/pvAccess.cpp',

  'src/factory/ChannelAccessFactory.cpp',

  'src/pipelineService/pipelineServer.cpp',
  'src/pipelineService/pipelineService.cpp',

  'src/pva/clientFactory.cpp',
  'src/pva/pvaVersion.cpp',

  'src/remote/abstractResponseHandler.cpp',
  'src/remote/beaconHandler.cpp',
  'src/remote/blockingTCPAcceptor.cpp',
  'src/remote/blockingTCPConnector.cpp',
  'src/remote/blockingUDPConnector.cpp',
  'src/remote/blockingUDPTransport.cpp',
  'src/remote/channelSearchManager.cpp',
  'src/remote/codec.cpp',
  'src/remote/security.cpp',
  'src/remote/serializationHelper.cpp',
  'src/remote/transportRegistry.cpp',

  'src/remoteClient/clientContextImpl.cpp',

  'src/rpcClient/rpcClient.cpp',

  'src/rpcService/rpcServer.cpp',
  'src/rpcService/rpcService.cpp',

  'src/server/baseChannelRequester.cpp',
  'src/server/beaconEmitter.cpp',
  'src/server/beaconServerStatusProvider.cpp',
  'src/server/responseHandlers.cpp',
  'src/server/server.cpp',
  'src/server/serverChannelImpl.cpp',
  'src/server/serverContext.cpp',
  'src/server/sharedstate_channel.cpp',
  'src/server/sharedstate_put.cpp',
  'src/server/sharedstate_pv.cpp',
  'src/server/sharedstate_rpc.cpp',

  'src/utils/configuration.cpp',
  'src/utils/getgroups.cpp',
  'src/utils/hexDump.cpp',
  'src/utils/inetAddressUtil.cpp',
  'src/utils/introspectionRegistry.cpp',
  'src/utils/logger.cpp',
  'src/utils/referenceCountingLock.cpp',
  'src/utils/requester.cpp',
  'src/utils/wildcard.cpp',
]

include_directories = [
  # For pvaVersionNum.h HACK
  '.',

  'src/client',
  'src/factory',
  'src/mb',
  'src/pipelineService',
  'src/pva',
  'src/remote',
  'src/remoteClient',
  'src/rpcClient',
  'src/rpcService',
  'src/server',
  'src/utils',
]

libpv_access = both_libraries(
  'epics-pv-access',
  sources,
  include_directories: include_directories,
  dependencies: [epics_com_dep, epics_pv_data_dep],
  install: true,
)

epics_pv_access_dep = declare_dependency(
  include_directories: include_directories,
  link_with: libpv_access,
)

tests = [
  ['channel_access_if_test', 'testApp/remote/channelAccessIFTest.cpp'],
  # depends on local iface
  #['pipeline_service_example', 'testApp/remote/pipelineServiceExample.cpp'],
  # depends on local iface
  #['rpc_client_example', 'testApp/remote/rpcClientExample.cpp'],
  # depends on local iface
  #['rpc_service_async_example', 'testApp/remote/rpcServiceAsyncExample.cpp'],
  # depends on local iface
  #['rpc_service_example', 'testApp/remote/rpcServiceExample.cpp'],
  # depends on local iface
  #['rpc_wild_service_example', 'testApp/remote/rpcWildServiceExample.cpp'],
  # depends on config env EPICS_IOC_LOG_FILE_NAME
  #['test_adc_sim', 'testApp/remote/testADCSim.cpp'],
  # depends on local iface
  #['test_channel_connect', 'testApp/remote/testChannelConnect.cpp'],
  ['test_client_factory', 'testApp/remote/testClientFactory.cpp'],
  ['test_codec', 'testApp/remote/testCodec.cpp'],
  # depends on local iface
  #['test_get_performance', 'testApp/remote/testGetPerformance.cpp'],
  # depends on local iface
  #['test_monitor_performance', 'testApp/remote/testMonitorPerformance.cpp'],
  # TODO: doesn't compile?
  #['test_nt_image', 'testApp/remote/testNTImage.cpp'],
  ['test_raii', 'testApp/remote/testRAII.cpp'],
  ['test_rpc', 'testApp/remote/testRPC.cpp'],
  # depends on local iface
  #['test_remote_client_impl', 'testApp/remote/testRemoteClientImpl.cpp'],
  # depends on local iface
  #['test_server', 'testApp/remote/testServer.cpp'],
  # depends on local iface
  #['test_server_context', 'testApp/remote/testServerContext.cpp'],
  ['testmonitorfifo', 'testApp/remote/testmonitorfifo.cpp'],
  ['testsharedstate', 'testApp/remote/testsharedstate.cpp'],
  ['configuration_test', 'testApp/utils/configurationTest.cpp'],
  ['showauth', 'testApp/utils/showauth.cpp'],
  ['test_atomic_boolean', 'testApp/utils/testAtomicBoolean.cpp'],
  ['test_fair_queue', 'testApp/utils/testFairQueue.cpp'],
  ['test_hex_dump', 'testApp/utils/testHexDump.cpp'],
  # depends on local iface
  #['test_inet_address_utils', 'testApp/utils/testInetAddressUtils.cpp'],
  ['test_wildcard', 'testApp/utils/testWildcard.cpp'],
]

foreach t : tests
  exe = executable(
    t[0],
    t[1],
    dependencies: [epics_com_dep, epics_pv_data_dep, epics_pv_access_dep]
  )
  test(t[0], exe, is_parallel: true)
endforeach

pkgconfig = import('pkgconfig')
pkgconfig.generate(libpv_access)

install_headers(headers, subdir: 'pv')

subdir('src/ca')
subdir('src/ioc')

subdir('pvtoolsSrc')

epics_ca_dep = dependency('epics-ca')

ca_headers = [
  'pv/caProvider.h',
]

ca_sources = [
 'caProvider.cpp',
 'caContext.cpp',
 'caChannel.cpp',
 'dbdToPv.cpp',
 'notifierConveyor.cpp',
]

libpv_access_ca = both_libraries(
  'epics-pv-access-ca',
  ca_sources,
  dependencies: [
    epics_ca_dep,
    epics_com_dep,
    epics_pv_access_dep,
    epics_pv_data_dep,
  ],
  install: true,
)

epics_pv_access_ca_dep = declare_dependency(
  include_directories: '.',
  link_with: libpv_access_ca,
)

tests = [
  ['ca_test_conveyor', '../../testCa/testConveyor.cpp'],
  # TODO: depends on the 'database' module
  #['ca_test_provider', '../../testCa/testCaProvider.cpp'],
]

foreach t : tests
  exe = executable(
    t[0],
    t[1],
    dependencies: [epics_com_dep, epics_pv_access_ca_dep]
  )
  test(t[0], exe, is_parallel: true)
endforeach

pkgconfig.generate(libpv_access_ca)

install_headers(ca_headers, subdir: 'pv')

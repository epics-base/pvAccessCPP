ioc_headers = [
  'pv/iocreftrack.h',
  'pv/iocshelper.h',
  'pv/syncChannelFind.h',
]

ioc_sources = [
  'PVAClientRegister.cpp',
  'PVAServerRegister.cpp',
  'reftrackioc.cpp',
]

libpv_access_ioc = both_libraries(
  'epics-pv-access-ioc',
  ioc_sources,
  # For pvaVersionNum.h HACK
  include_directories: '../..',
  dependencies: [epics_com_dep, epics_pv_data_dep, epics_pv_access_dep],
  install: true,
)

pkgconfig.generate(libpv_access_ioc)

install_headers(ioc_headers, subdir: 'pv')

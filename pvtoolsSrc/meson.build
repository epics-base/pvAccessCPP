foreach e : [
  ['pvget', ['./pvget.cpp', './pvutils.cpp']],
  ['pvmonitor', ['./pvmonitor.cpp', './pvutils.cpp']],
  ['pvput', ['./pvput.cpp', './pvutils.cpp']],
  ['pvcall', ['./pvcall.cpp', './pvutils.cpp']],
  ['pvinfo', ['./pvinfo.cpp', './pvutils.cpp']],
  ['pvlist', './pvlist.cpp'],
]
  executable(
    e[0],
    e[1],
    dependencies: [
      epics_com_dep,
      epics_pv_access_ca_dep,
      epics_pv_access_dep,
      epics_pv_data_dep,
    ],
    install: true,
  )
endforeach

#!/bin/sh
set -e -x

CURDIR="$PWD"

cat << EOF > $CURDIR/configure/RELEASE.local
EPICS_BASE=$HOME/.source/epics-base
EOF

install -d "$HOME/.source"
cd "$HOME/.source"

add_gh_flat() {
  MODULE=$1
  REPOOWNER=$2
  REPONAME=$3
  BRANCH=$4
  MODULE_UC=$(echo $MODULE | tr 'a-z' 'A-Z')
  ( git clone --quiet --depth 5 --branch $BRANCH https://github.com/$REPOOWNER/$REPONAME.git $MODULE && \
  cd $MODULE && git log -n1 )
  cat < $CURDIR/configure/RELEASE.local > $MODULE/configure/RELEASE.local
  cat << EOF >> $CURDIR/configure/RELEASE.local
${MODULE_UC}=$HOME/.source/$MODULE
EOF
}

# not recursive
git clone --quiet --depth 5 --branch "$BRBASE" https://github.com/${REPOBASE:-epics-base}/epics-base.git epics-base
(cd epics-base && git log -n1 )
add_gh_flat pvData ${REPOPVD:-epics-base} pvDataCPP ${BRPVD:-master}

if [ -e $CURDIR/configure/RELEASE.local ]
then
  cat $CURDIR/configure/RELEASE.local
fi

EPICS_HOST_ARCH=`sh epics-base/startup/EpicsHostArch`

# requires wine and g++-mingw-w64-i686
if [ "$WINE" = "32" ]
then
  echo "Cross mingw32"
  sed -i -e '/CMPLR_PREFIX/d' epics-base/configure/os/CONFIG_SITE.linux-x86.win32-x86-mingw
  cat << EOF >> epics-base/configure/os/CONFIG_SITE.linux-x86.win32-x86-mingw
CMPLR_PREFIX=i686-w64-mingw32-
EOF
  cat << EOF >> epics-base/configure/CONFIG_SITE
CROSS_COMPILER_TARGET_ARCHS+=win32-x86-mingw
EOF
fi

if [ "$STATIC" = "YES" ]
then
  echo "Build static libraries/executables"
  cat << EOF >> epics-base/configure/CONFIG_SITE
SHARED_LIBRARIES=NO
STATIC_BUILD=YES
EOF
fi

case "$CMPLR" in
clang)
  echo "Host compiler is clang"
  cat << EOF >> epics-base/configure/os/CONFIG_SITE.Common.$EPICS_HOST_ARCH
GNU         = NO
CMPLR_CLASS = clang
CC          = clang
CCC         = clang++
EOF

  # hack
  sed -i -e 's/CMPLR_CLASS = gcc/CMPLR_CLASS = clang/' epics-base/configure/CONFIG.gnuCommon

  clang --version
  ;;
*)
  echo "Host compiler is default"
  gcc --version
  ;;
esac

cat <<EOF >> epics-base/configure/CONFIG_SITE
USR_CPPFLAGS += $USR_CPPFLAGS
USR_CFLAGS += $USR_CFLAGS
USR_CXXFLAGS += $USR_CXXFLAGS
EOF

# set RTEMS to eg. "4.9" or "4.10"
# requires qemu, bison, flex, texinfo, install-info
if [ -n "$RTEMS" ]
then
  echo "Cross RTEMS${RTEMS} for pc386"
  curl -L "https://github.com/mdavidsaver/rsb/releases/download/20171203-${RTEMS}/i386-rtems${RTEMS}-trusty-20171203-${RTEMS}.tar.bz2" \
  | tar -C / -xmj

  sed -i -e '/^RTEMS_VERSION/d' -e '/^RTEMS_BASE/d' epics-base/configure/os/CONFIG_SITE.Common.RTEMS
  cat << EOF >> epics-base/configure/os/CONFIG_SITE.Common.RTEMS
RTEMS_VERSION=$RTEMS
RTEMS_BASE=$HOME/.rtems
EOF
  cat << EOF >> epics-base/configure/CONFIG_SITE
CROSS_COMPILER_TARGET_ARCHS += RTEMS-pc386-qemu
EOF
fi

make -j2 -C epics-base $EXTRA

if [ "$BRBASE" ]
then
  make -j2 -C pvData $EXTRA
fi

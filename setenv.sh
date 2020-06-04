
if [ "$1" = "" ]
then
  echo "Usage:"
  echo "source `basename $0` <target>"
  echo ""
  echo "Available targets:"
  ls -1 *.mk | sed -e 's/config-/\t/g;s/.mk$//g'
  echo ""
  exit 1
fi

export PREFIX=/usr/local/urmetiotc_x86_64
export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig
export LD_LIBRARY_PATH=$PREFIX/lib
export TARGET=$1


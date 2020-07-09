
if [ "$1" = "" ]
then
  echo "Usage:"
  echo "source `basename $0` <target>"
  echo ""
  echo "Available targets:"
  ls -1 *.mk | sed -e 's/config-/\t/g;s/.mk$//g'
  echo ""
  exit 1
elif [ "$1" = "android" ]
then
  export ANDROID_NDK_ROOT=/usr/local/android-ndk-r10e-urmetiotc
  export SYSROOT=${ANDROID_NDK_ROOT}/platforms/android-9/arch-arm
  export PATH=${ANDROID_NDK_ROOT}/toolchains/arm-linux-androideabi-4.8/prebuilt/linux-x86_64/bin:${SYSROOT}/usr/bin:${PATH}
  cp -f libiotc-android.mri libiotc.mri
  export TARGET=android

else
  export PREFIX=/usr/local/urmetiotc_x86_64
  export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig
  export LD_LIBRARY_PATH=$PREFIX/lib
  cp -f libiotc-linux.mri libiotc.mri
  export TARGET=$1
fi


export ANDROID_NDK_ROOT=/usr/local/android-ndk-r10e
export SYSROOT=${ANDROID_NDK_ROOT}/platforms/android-9/arch-arm
export PATH=${ANDROID_NDK_ROOT}/toolchains/arm-linux-androideabi-4.8/prebuilt/linux-x86_64/bin:${SYSROOT}/usr/bin:${PATH}
export TARGET=android


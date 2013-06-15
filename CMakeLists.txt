#
# For compilation, the following environment vars should be defined before running cmake:
# 	USERLANDPATH	- path to the root of the Pi userland files (from https://github.com/raspberrypi/userland.git)
#
# For cross-compilation, the following environment vars should be defined before running cmake:
# 	ROOTFSPATH		- path to the root of the Pi file system (for include and library file paths)
# 	TOOLPATH 	- path to the root of the cross-compiler (e.g. a path ending in "tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian" for the Linaro toolchain)
# 	TOOLPREFIX	- platform specific prefix for compiler tool names (e.g. arm-linux-gnueabihf-)
#
# Optional environment vars:
#	DEBUG		- if set to any value, will build debug version (no optimisation, full symbol information).
#

project(motion)
cmake_minimum_required(VERSION 2.8)

#set(TOOLPATH /home/ntuckett/RPi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian)
#set(ROOTFSPATH /home/ntuckett/RPi/rootfs-2013-05)
#set(USERLANDPATH /home/ntuckett/RPi/userland)
#set(TOOLPREFIX arm-linux-gnueabihf-)

set(ROOTFSPATH $ENV{ROOTFSPATH})
set(USERLANDPATH $ENV{USERLANDPATH})

execute_process(COMMAND uname -m OUTPUT_VARIABLE MACHINE)

if(NOT MACHINE MATCHES "arm*")
	set(TOOLPATH $ENV{TOOLPATH})
	set(TOOLPREFIX $ENV{TOOLPREFIX})
	set(CMAKE_C_COMPILER ${TOOLPATH}/bin/${TOOLPREFIX}gcc)
	set(CMAKE_SYSTEM_NAME Linux)

	include_directories(
						${TOOLPATH}/arm-linux-gnueabihf/include 
						${TOOLPATH}/arm-linux-gnueabihf/libc/usr/include/arm-linux-gnueabihf 
						${TOOLPATH}/arm-linux-gnueabihf/libc/usr/include
						${TOOLPATH}/lib/gcc/arm-linux-gnueabihf/4.7.2/include-fixed
						${TOOLPATH}/lib/gcc/arm-linux-gnueabihf/4.7.2/include
						${TOOLPATH}/lib/gcc/arm-linux-gnueabihf/4.7.2/finclude
						${ROOTFSPATH}/usr/include/arm-linux-gnueabihf
						${ROOTFSPATH}/usr/include
						${ROOTFSPATH}/usr/local/include
						)

	add_definitions(
					-march=armv6zk 
					-mfpu=vfp 
					-mfloat-abi=hard
					)
					
	link_directories(
					${TOOLPATH}/arm-linux-gnueabihf/lib
					${TOOLPATH}/arm-linux-gnueabihf/libc/lib
					${TOOLPATH}/arm-linux-gnueabihf/libc/lib/arm-linux-gnueabihf
					${TOOLPATH}/lib/gcc/arm-linux-gnueabihf/4.7.2
					${TOOLPATH}/libexec/gcc/arm-linux-gnueabihf/4.7.2
					${ROOTFSPATH}/lib/arm-linux-gnueabihf
					${ROOTFSPATH}/lib
					${ROOTFSPATH}/usr/lib/arm-linux-gnueabihf
					${ROOTFSPATH}/usr/lib
					${ROOTFSPATH}/usr/local/lib
					)					
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(
					${ROOTFSPATH}/usr/include/mysql
					${ROOTFSPATH}/usr/include/postgresql
					${USERLANDPATH}
					${USERLANDPATH}/interface/vcos
					${USERLANDPATH}/interface/vcos/pthreads
					${USERLANDPATH}/interface/vmcs_host/linux
					${USERLANDPATH}/interface/vmcs_host
					${USERLANDPATH}/host_applications/linux/libs/bcm_host/include
					)
					
add_definitions(	-DVERSION="mmaltest"
					-Dsysconfdir="/etc"
					-D_REENTRANT
					-DHAVE_FFMPEG
					-DFFMPEG_NEW_INCLUDES
					-DHAVE_FFMPEG_NEW
					-DMOTION_V4L2
					-DMOTION_V4L2_OLD
					-DTYPE_32BIT=int
					-DHAVE_BSWAP
					-Wall 
					-c 
					-fmessage-length=0 
					-std=gnu99 
					)
					
if ($ENV{DEBUG})
	add_definitions(-O0 -g3 -D_DEBUG)
else()
	add_definitions(-O3)
endif()					
					
link_directories(
				${ROOTFSPATH}/opt/vc/lib
				)
					
add_executable(motion 	
				alg.c
				conf.c
				draw.c
				event.c
				ffmpeg.c
				jpegutils.c
				logger.c
				md5.c
				mmalcam.c
				motion.c
				netcam.c
				netcam_ftp.c
				netcam_jpeg.c
				netcam_wget.c
				picture.c
				rotate.c
				stream.c
				track.c
				video2.c
				video.c
				video_common.c
				vloopback_motion.c
				webhttpd.c
				raspicam/RaspiCamControl.c
				raspicam/RaspiCLI.c
				)

target_link_libraries(motion	
						pthread
						m
						jpeg
						avformat
						avcodec
						avutil
						z
						mysqlclient
						pq
						rt
						bcm_host
						vcos
						vchiq_arm
						mmal_core
						mmal_util
						mmal_vc_client
						-Xlinker -rpath-link=${ROOTFSPATH}/usr/lib/arm-linux-gnueabihf -Xlinker -rpath-link=${ROOTFSPATH}/lib/arm-linux-gnueabihf
						)


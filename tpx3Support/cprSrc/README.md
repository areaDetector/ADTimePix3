# cpr on older OS 
The Curl for humans (cpr) library requires newer cmake and compiler. These steps are to help with compilation process. The steps are general for other older operating system, but developed for specifically RHEL7.9.

## Dependencies
* cmake version > 3.15
* gcc >= 7.1 is needed for cpr
* ubuntu/debian
    * /usr/include/libssh2.h 
        * sudo apt install libssh2-1-dev
    * curl/curlver.h 
        * sudo apt install libcurl4-openssl-dev
        * sudo apt install curl
    * sudo apt install libssl-dev

## RHEL 7.9
* default cmake is 3.0.2
* default gcc is 4.8.5
```
[kg1@bl100-dasopi1 tpx3Support]$ cmake --version
cmake version 3.0.2
[kg1@bl100-dasopi1 base]$ gcc --version
gcc (GCC) 4.8.5 20150623 (Red Hat 4.8.5-44)
```
    * download cmake script: cmake-3.25.2-linux-x86_64.sh
      * https://cmake.org/download/
    * ./cmake-3.25.2-linux-x86_64.sh --prefix=/SNS/users/kg1/epics/cmake_dir 
* dependency
  * sudo yum install libssh2-devel.x86_64
  * sudo yum install openssl-devel.x86_64
  * sudo yum install libcurl-devel.x86_64 
  * These could be needed 
    * curl.x86_64, 
    * libcurl.x86_64
* follow installCPR.sh script using local cmake
    * git clone https://github.com/libcpr/cpr
    * cd cpr
    * mkdir build
    * cd build
    * /SNS/users/kg1/epics/cmake_dir/bin/cmake ..
    * /SNS/users/kg1/epics/cmake_dir/bin/cmake --build .
    * cp cpr_generated_includes/cpr/cprver.h ../include/cpr/.
* gcc 9 (in local terminal)
    * devtoolset
        * https://www.softwarecollections.org/en/scls/rhscl/devtoolset-7/
    * sudo yum install devtoolset-9 
    * scl enable devtoolset-9 bash
    * ADTimePix3 requires newer compiler
        * gcc 9 (add following lines)
          * $(EPICS_BASE)/configure/os/CONFIG.linux-x86_64.linux-x86_64
            * CC=/opt/rh/devtoolset-9/root/usr/bin/g++
            * GCC=/opt/rh/devtoolset-9/root/usr/bin/gcc
* curl/system.h (ADTimePix3 compile: point to curl headers)
    * export C_INCLUDE_PATH=/opt/puppetlabs/puppet/include
    * export CPLUS_INCLUDE_PATH=/opt/puppetlabs/puppet/include





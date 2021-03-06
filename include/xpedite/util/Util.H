///////////////////////////////////////////////////////////////////////////////
//
// A collection of utility methods to
//
//  1. capture and build stack trace of the calling thread
//  2. Pin threads to a cpu
//  3. Methods to build file system paths for different data files
//
// Author: Manikandan Dhamodharan, Morgan Stanley
//
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include <xpedite/log/Log.H>
#include <unistd.h>
#include <sys/syscall.h>
#include <cstring>
#include <stdexcept>
#include <cstdlib>
#include <sstream>
#include <string>
#include <sstream>
#include <sys/prctl.h>
#include <cstdio>
#include <iostream>
#include <fcntl.h>
#include <pthread.h>
#include <execinfo.h>
#include <signal.h>

namespace xpedite { namespace util {

  inline pid_t gettid() {
    return syscall(__NR_gettid);
  }

  inline void pinThisThread(unsigned cpu_) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_, &cpuset);
    if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset)) {
      throw std::runtime_error {"xpedite - failed to pin thread"};
    }
  }

  inline std::string buildStackTrace() {
    // To capture trace, the statically lined symbols must be exported to dynsym
    // use -rdynamic flag to export all symbols to dynsym section
    void* btList[128];
    auto size = backtrace(btList, sizeof(btList)/sizeof(btList[0]));
    auto btStrList = backtrace_symbols(btList, size);
    if(btStrList == nullptr) {
      return "failed to read backtrace.";
    }

    std::ostringstream stream;
    for(decltype(size) i=0; i<size; ++i) {
      stream << btStrList[i] << std::endl;
    }
    free(btStrList);
    return stream.str();
  }

  inline std::string getProcessName() {
    char tname[32];
    memset(tname, 0, sizeof(tname));
    prctl(PR_GET_NAME, &tname);
    return tname;
  }

  inline std::string getExecutablePath() {
    char buf[4096]; 
    ssize_t len = readlink("/proc/self/exe", buf, 4095);
    if(len == -1) {
      XpediteLogCritical << "xpedite - error getting binary path" << XpediteLogEnd;
      return "unknown";
    }  
    return buf;
  }

  inline std::string buildSamplesFilePath(const char* prefix_, bool suffixPid_, bool suffixTid_) {
    auto processName = getProcessName();
    std::ostringstream stream;
    stream << "/dev/shm/" << prefix_ << "-" << processName;
    if(suffixPid_) stream << "-" << getpid();
    if(suffixTid_) stream << "-" << gettid();
    stream << ".data";
    return stream.str();
  }

  inline int openSamplesFile(const std::string& fname_) {
    auto fd = open(fname_.c_str(), O_WRONLY |O_APPEND |O_TRUNC |O_CREAT, 0644);
    if(fd < 0) {
      std::cerr << "xpedite - error opening samples file '" << fname_ << "' error(" << errno << ") - " << strerror(errno) << std::endl;
    }
    return fd;
  }

  inline void faultHandler(int no_) {
    std::cerr << "--------------------------------------------------------------" << std::endl;
    std::cerr << "terminated by signal (" << no_ << ")" << std::endl;
    std::cerr << buildStackTrace() << std::endl;
    std::cerr << "--------------------------------------------------------------" << std::endl;
    std::exit(no_);
  }

  inline void installFaultHandler() {
    struct sigaction action {};
    action.sa_handler = faultHandler;
    if(sigaction(SIGSEGV, &action, nullptr)) {
      throw std::runtime_error {"failed to register fault handler"};
    }
  }

}}

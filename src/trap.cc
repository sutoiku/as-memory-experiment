#include <iostream>
#include <node.h>
#include <v8.h>
#include <signal.h>

#include "trap.hh"

const char *get_sig(int sig) {
  switch(sig) {
    case SIGSEGV: return "SIGSEGV";
    case SIGBUS: return "SIGBUS";
    default: return "<unknown signal>";
  }
}

static void signal_handler(int sig, siginfo_t *info, void *ucontext) {

  if(v8::V8::TryHandleSignal(sig, info, ucontext)) { // TryHandleWebAssemblyTrapPosix
    return;
  }

  std::cout << get_sig(sig) << " " << info->si_addr << std::endl;

  // re-throw
  signal(sig, SIG_DFL);
  raise(sig);
}

namespace experiment {
  void setup_trap() {
    v8::V8::EnableWebAssemblyTrapHandler(false);

    struct sigaction action;
    action.sa_sigaction = signal_handler;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    // {sigaction} installs a new custom segfault handler. On success, it returns
    // 0. If we get a nonzero value, we report an error to the caller by returning
    // false.

    // SIGSEGV on linux
    sigaction(SIGSEGV, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
  }
}

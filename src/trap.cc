#include <iostream>
#include <node.h>
#include <v8.h>
#include <signal.h>

#include "trap.hh"
#include "vm.hh"

static const char *get_sig(int sig) {
  switch(sig) {
    case SIGSEGV: return "SIGSEGV";
    case SIGBUS: return "SIGBUS";
    default: return "<unknown signal>";
  }
}

static void signal_handler(int sig, siginfo_t *info, void *ucontext) {

  // on OSX v8 handle SIGBUS only
  if(sig == SIGBUS || sig == SIGSEGV) {

    ucontext_t* uc = reinterpret_cast<ucontext_t*>(ucontext);
    auto* context_rip = &uc->uc_mcontext->__ss.__rip; // OSX only, cf v8 code for linux
    uintptr_t fault_addr = *context_rip;
    std::cout << get_sig(sig) << " " << info->si_addr << " from instruction at " << reinterpret_cast<void *>(fault_addr) << std::endl;

    if(experiment::handle_cow(reinterpret_cast<uintptr_t>(info->si_addr))) {
      // TODO: do we need something to retry?
      return;
    }

    // on OSX v8 handle SIGBUS only
    if (v8::V8::TryHandleSignal(SIGBUS, info, ucontext)) { // TryHandleWebAssemblyTrapPosix
      std::cout << "TryHandleSignal success" << std::endl;
      return;
    }
  }

  std::cout << get_sig(sig) << " UNCAUGHT" << std::endl;


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

//===-- Signposts.cpp - Interval debug annotations ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/Signposts.h"
#include "llvm/Support/Timer.h"

#if LLVM_SUPPORT_XCODE_SIGNPOSTS
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/Mutex.h"
#endif // if LLVM_SUPPORT_XCODE_SIGNPOSTS

using namespace llvm;

#if LLVM_SUPPORT_XCODE_SIGNPOSTS
namespace {
os_log_t *LogCreator() {
  os_log_t *X = new os_log_t;
  *X = os_log_create("org.llvm.signposts", OS_LOG_CATEGORY_POINTS_OF_INTEREST);
  return X;
}
struct LogDeleter {
  void operator()(os_log_t *X) const {
    os_release(*X);
    delete X;
  }
};
} // end anonymous namespace

namespace llvm {
class SignpostEmitterImpl {
  using LogPtrTy = std::unique_ptr<os_log_t, LogDeleter>;

  LogPtrTy SignpostLog;
  DenseMap<const void *, os_signpost_id_t> Signposts;
  sys::SmartMutex<true> Mutex;

public:
  os_log_t &getLogger() const { return *SignpostLog; }
  os_signpost_id_t getSignpostForObject(const void *O) {
    sys::SmartScopedLock<true> Lock(Mutex);
    const auto &I = Signposts.find(O);
    if (I != Signposts.end())
      return I->second;
    os_signpost_id_t ID = {};
    if (SIGNPOSTS_AVAILABLE()) {
      ID = os_signpost_id_make_with_pointer(getLogger(), O);
    }
    const auto &Inserted = Signposts.insert(std::make_pair(O, ID));
    return Inserted.first->second;
  }

  SignpostEmitterImpl() : SignpostLog(LogCreator()) {}

  bool isEnabled() const {
    if (SIGNPOSTS_AVAILABLE())
      return os_signpost_enabled(*SignpostLog);
    return false;
  }

  void startInterval(const void *O, llvm::StringRef Name) {
    if (isEnabled()) {
      if (SIGNPOSTS_AVAILABLE()) {
        // Both strings used here are required to be constant literal strings.
        os_signpost_interval_begin(getLogger(), getSignpostForObject(O),
                                   "LLVM Timers", "%s", Name.data());
      }
    }
  }

  void endInterval(const void *O) {
    if (isEnabled()) {
      if (SIGNPOSTS_AVAILABLE()) {
        // Both strings used here are required to be constant literal strings.
        os_signpost_interval_end(getLogger(), getSignpostForObject(O),
                                 "LLVM Timers", "");
      }
    }
  }
};
} // end namespace llvm
#else
/// Definition necessary for use of std::unique_ptr in SignpostEmitter::Impl.
class llvm::SignpostEmitterImpl {};
#endif // if LLVM_SUPPORT_XCODE_SIGNPOSTS

#if LLVM_SUPPORT_XCODE_SIGNPOSTS
#define HAVE_ANY_SIGNPOST_IMPL 1
#else
#define HAVE_ANY_SIGNPOST_IMPL 0
#endif

SignpostEmitter::SignpostEmitter() {
#if HAVE_ANY_SIGNPOST_IMPL
  Impl = std::make_unique<SignpostEmitterImpl>();
#endif // if !HAVE_ANY_SIGNPOST_IMPL
}

SignpostEmitter::~SignpostEmitter() = default;

bool SignpostEmitter::isEnabled() const {
#if HAVE_ANY_SIGNPOST_IMPL
  return Impl->isEnabled();
#else
  return false;
#endif // if !HAVE_ANY_SIGNPOST_IMPL
}

void SignpostEmitter::startInterval(const void *O, StringRef Name) {
#if HAVE_ANY_SIGNPOST_IMPL
  if (Impl == nullptr)
    return;
  return Impl->startInterval(O, Name);
#endif // if !HAVE_ANY_SIGNPOST_IMPL
}

#if HAVE_ANY_SIGNPOST_IMPL
os_log_t &SignpostEmitter::getLogger() const { return Impl->getLogger(); }
os_signpost_id_t SignpostEmitter::getSignpostForObject(const void *O) {
  return Impl->getSignpostForObject(O);
}
#endif

void SignpostEmitter::endInterval(const void *O) {
#if HAVE_ANY_SIGNPOST_IMPL
  if (Impl == nullptr)
    return;
  Impl->endInterval(O);
#endif // if !HAVE_ANY_SIGNPOST_IMPL
}

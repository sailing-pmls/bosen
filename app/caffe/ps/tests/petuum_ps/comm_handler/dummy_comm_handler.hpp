#ifndef PETUUM_DUMMY_COMM_HANDLER
#define PETUUM_DUMMY_COMM_HANDLER

#include "petuum_ps/comm_handler/comm_handler.hpp"
#include <glog/logging.h>

namespace petuum {

namespace {

// Use default constructor.
const ConfigParam kCommConfigParam;

}   // anonymous namespace

// DummyCommHandler provides a way to mock out CommHandler in classes that
// depends on it.
class DummyCommHandler : public CommHandler {
  public:
    // Easy default constructor.
    DummyCommHandler();
};

DummyCommHandler::DummyCommHandler()
  : CommHandler(kCommConfigParam) {
    LOG(INFO) << "DummyCommHandler created.";
  }

}   // namespace petuum

#endif  // PETUUM_DUMMY_COMM_HANDLER

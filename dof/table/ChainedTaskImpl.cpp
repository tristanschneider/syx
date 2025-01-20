#include "Precompile.h"
#include "ChainedTaskImpl.h"
#include "AppBuilder.h"

ChainedTaskImpl::ChainedTaskImpl(std::unique_ptr<ITaskImpl> p)
  : parent{ std::move(p) }
{
}

ChainedTaskImpl::~ChainedTaskImpl() = default;

void ChainedTaskImpl::setWorkerCount(size_t count) {
  parent->setWorkerCount(count);
}

AppTaskMetadata ChainedTaskImpl::init(RuntimeDatabase& db) {
  return parent->init(db);
}

void ChainedTaskImpl::initThreadLocal(AppTaskArgs& args) {
  parent->initThreadLocal(args);
}

void ChainedTaskImpl::execute(AppTaskArgs& args) {
  parent->execute(args);
}

std::shared_ptr<AppTaskConfig> ChainedTaskImpl::getConfig() {
  return parent->getConfig();
}

AppTaskPinning::Variant ChainedTaskImpl::getPinning() {
  return parent->getPinning();
}

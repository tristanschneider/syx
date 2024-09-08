#include "Precompile.h"
#include "NotifyingTableModifier.h"

#include "AppBuilder.h"
#include "DBEvents.h"

NotifyingTableModifier::NotifyingTableModifier(RuntimeDatabaseTaskBuilder& task, const TableID& table)
  : modifier{ task.getModifierForTable(table) }
  , stable{ &task.query<const StableIDRow>(table).get<0>(0) }
{
}

NotifyingTableModifier::~NotifyingTableModifier() = default;

void NotifyingTableModifier::initTask(AppTaskArgs& a) {
  args = &a;
}

const ElementRef* NotifyingTableModifier::addElements(size_t count) {
  assert(args);

  const size_t begin = modifier->addElements(count);
  for(size_t i = begin; i < begin + count; ++i) {
    Events::onNewElement(stable->at(i), *args);
  }
  return &stable->at(begin);
}

size_t NotifyingTableModifier::toIndex(const ElementRef& e) const {
  return static_cast<size_t>(&e - &stable->at(0));
}

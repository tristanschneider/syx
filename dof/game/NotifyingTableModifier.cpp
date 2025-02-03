#include "Precompile.h"
#include "NotifyingTableModifier.h"

#include "AppBuilder.h"
#include "Events.h"

NotifyingTableModifier::NotifyingTableModifier(RuntimeDatabaseTaskBuilder& task, const TableID& table)
  : modifier{ task.getModifierForTable(table) }
  , stable{ &task.query<const StableIDRow>(table).get<0>(0) }
  , events{ &task.query<Events::EventsRow>(table).get<0>(0) }
{
}

NotifyingTableModifier::~NotifyingTableModifier() = default;

void NotifyingTableModifier::initTask(AppTaskArgs&) {
}

const ElementRef* NotifyingTableModifier::addElements(size_t count) {
  const size_t begin = modifier->addElements(count);
  for(size_t i = begin; i < begin + count; ++i) {
    events->getOrAdd(i).setCreate();
  }
  return &stable->at(begin);
}

size_t NotifyingTableModifier::toIndex(const ElementRef& e) const {
  return static_cast<size_t>(&e - &stable->at(0));
}

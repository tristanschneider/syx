#pragma once

struct AppTaskArgs;
struct StableIDRow;
class ITableModifier;
class RuntimeDatabaseTaskBuilder;
class ElementRef;
struct TableID;

namespace Events {
  struct EventsRow;
}

class NotifyingTableModifier {
public:
  NotifyingTableModifier(RuntimeDatabaseTaskBuilder& task, const TableID& table);
  ~NotifyingTableModifier();

  void initTask(AppTaskArgs& a);

  const ElementRef* addElements(size_t count);
  size_t toIndex(const ElementRef& e) const;

private:
  const StableIDRow* stable{};
  Events::EventsRow* events{};
  std::shared_ptr<ITableModifier> modifier;
};
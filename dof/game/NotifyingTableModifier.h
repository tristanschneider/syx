#pragma once

struct AppTaskArgs;
struct StableIDRow;
class ITableModifier;
class RuntimeDatabaseTaskBuilder;
class ElementRef;
struct TableID;

class NotifyingTableModifier {
public:
  NotifyingTableModifier(RuntimeDatabaseTaskBuilder& task, const TableID& table);
  ~NotifyingTableModifier();

  void initTask(AppTaskArgs& a);

  const ElementRef* addElements(size_t count);
  size_t toIndex(const ElementRef& e) const;

private:
  AppTaskArgs* args{};
  const StableIDRow* stable{};
  std::shared_ptr<ITableModifier> modifier;
};
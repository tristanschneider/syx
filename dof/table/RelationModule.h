#pragma once

#include <StableElementID.h>
#include <Table.h>

struct AppTaskArgs;
class IAppModule;
class RuntimeDatabase;
class RuntimeTable;

//Parent/child relationships for tables.
//If a table has `HasParentRow` it can be used as a target for RelationWriter::addChildren,
//given a parent with `HasChildrenRow`
//Child table entries are removed whenever the parent expires using events
namespace Relation {
  struct ParentEntry {
    ElementRef parent;
  };
  struct ChildrenEntry {
    std::vector<ElementRef> children;
  };
  struct HasParentRow : Row<ParentEntry> {};
  struct HasChildrenRow : Row<ChildrenEntry> {};

  class RelationWriter {
  public:
    struct NewChildren {
      //Table that the new children are in
      RuntimeTable* table{};
      //Index of the first child in table, the rest are after this is order
      size_t startIndex{};
      //Array of refs to the new child entries, count matches the requested child count
      const ElementRef* childRefs{};
    };

    RelationWriter(AppTaskArgs& args);

    NewChildren addChildren(const ElementRef& parent, ChildrenEntry& children, TableID childTable, size_t count);

  private:
    RuntimeDatabase& localDB;
  };

  std::unique_ptr<IAppModule> createModule();
};
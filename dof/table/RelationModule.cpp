#include <Precompile.h>
#include <RelationModule.h>

#include <Events.h>
#include <IAppModule.h>
#include <RuntimeDatabase.h>
#include <TLSTaskImpl.h>

namespace Relation {
  RelationWriter::RelationWriter(AppTaskArgs& args)
    : localDB{ &args.getLocalDB() } {
  }

  RelationWriter::NewChildren RelationWriter::addChildren(const ElementRef& parent, ChildrenEntry& children, TableID childTable, size_t count) {
    NewChildren result;
    result.table = localDB ? localDB->tryGet(childTable) : nullptr;
    const StableIDRow* stable = result.table ? result.table->tryGet<const StableIDRow>() : nullptr;
    if(!result.table || !stable || !count) {
      return {};
    }

    result.startIndex = result.table->addElements(count);
    result.childRefs = &stable->at(result.startIndex);

    //Add children references in parent container so they can be used to find children for removal
    children.children.insert(children.children.end(), stable->begin() + result.startIndex, stable->begin() + result.startIndex + count);
    //Optionally, link child references to parent
    if(HasParentRow* hasParent = result.table->tryGet<HasParentRow>()) {
      for(size_t i = 0; i < count; ++i) {
        hasParent->at(i + result.startIndex).parent = parent;
      }
    }
    return result;
  }

  struct RemoveChildren {
    void init(RuntimeDatabaseTaskBuilder& task) {
      parentQuery = task;
      ids = task.getRefResolver();
      childResolver = task.getResolver(childEvents, parentChildEntries);
    }

    bool shouldRemoveChildren(const Events::ElementEvent& event) {
      //If the parent is being destroyed, children should as well
      if(event.isDestroy()) {
        return true;
      }
      //If the parent is being created, children should not be removed.
      //If the parent is moving to a table that no longer has the HasChildrenRow,
      //remove children now or the event query would otherwise miss the parent destruction.
      return event.isMove() && !childResolver->tryGetOrSwapRow(parentChildEntries, event.getTableID());
    }

    void execute() {
      //Iterate over all events on parent to see if children should be deleted
      for(size_t t = 0; t < parentQuery.size(); ++t) {
        auto&& [e, children, events] = parentQuery.get(t);
        for(auto&& event : events) {
          //If the event on the parent indicates that children should be deleted, request deletion on their event row
          if(shouldRemoveChildren(event.second)) {
            for(const ElementRef& child : children->at(event.first).children) {
              const auto childID = ids.unpack(child);
              if(childResolver->tryGetOrSwapRow(childEvents, childID)) {
                childEvents->getOrAdd(childID.getElementIndex()).setDestroy();
              }
            }
            //Children container could be cleared but it shouldn't matter because the table element is getting destroyed.
          }
        }
      }
    }

    QueryResult<
      const StableIDRow,
      const HasChildrenRow,
      const Events::EventsRow
    > parentQuery;
    ElementRefResolver ids;
    std::shared_ptr<ITableResolver> childResolver;
    CachedRow<Events::EventsRow> childEvents;
    CachedRow<const HasChildrenRow> parentChildEntries;
  };

  class RelationModule : public IAppModule {
  public:
    void preProcessEvents(IAppBuilder& app) final {
      app.submitTask(TLSTask::create<RemoveChildren>("RelationalEvents"));
    }
  };

  std::unique_ptr<IAppModule> createModule() {
    return std::make_unique<RelationModule>();
  }
};
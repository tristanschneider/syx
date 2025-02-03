#pragma once

#include "DatabaseID.h"
#include "SparseRow.h"
#include "Table.h"
#include <variant>

class IAppModule;

namespace Events {
  struct ElementEvent {
  public:
    ElementEvent() = default;

    //Safest way to emit an event is to do row.getOrAdd(i).setX() so that the event is appended to whatever else might have already been queued.
    void setCreate() {
      eventType |= CREATE_EVENT;
    }

    void setMove(TableID dst) {
      eventType |= MOVE_EVENT;
      table = dst.getTableIndex();
    }

    ElementEvent& setDestroy() {
      eventType |= DESTROY_EVENT;
    }

    auto operator<=>(const ElementEvent&) const = default;

    bool isCreate() const {
      return eventType & CREATE_EVENT;
    }

    bool isMove() const {
      return eventType & MOVE_EVENT;
    }

    bool isDestroy() const {
      return eventType & DESTROY_EVENT;
    }

    TableID getTableID() const {
      return TableID{}.remake(table, 0);
    }

  private:
    ElementEvent(uint8_t e)
      : eventType{ e } {
    }

    ElementEvent(TableIndex t, uint8_t e)
      : table{ t }
      , eventType{ e } {
    }

    //An element can be created and moved/destroyed in the same event, and the combination may be meaningful to consumers.
    static constexpr uint8_t CREATE_EVENT = 1;
    //Move and destroy can also happen together but presumably destruction would take precedence. Either way, both are noted.
    //When the move flag is set, the table index is valid
    static constexpr uint8_t MOVE_EVENT = 1 << 1;
    static constexpr uint8_t DESTROY_EVENT = 1 << 2;
    TableIndex table{ std::numeric_limits<TableIndex>::max() };
    uint8_t eventType{};
  };

  struct EventsRow : SparseRow<ElementEvent> {};

  //Creates a module that adds event rows to any tables with stable rows
  std::unique_ptr<IAppModule> createModule();
}

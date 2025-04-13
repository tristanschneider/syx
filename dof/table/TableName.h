#pragma once

#include "AppBuilder.h"
#include "Table.h"

namespace TableName {
  //Table can choose to expose a name for debugging purposes.
  //It is also used to expose a table for deserialization.
  struct TableName {
    std::string name;
  };
  struct TableNameRow : SharedRow<TableName> {
    static constexpr std::string_view KEY = "TableName";
  };

  //Set the name of the table that matches the filter
  template<class... Filter>
  void setName(IAppBuilder& builder, TableName name) {
    auto task = builder.createTask();
    task.setName("set table names");
    auto q = task.query<TableNameRow, const Filter...>();
    assert(q.size() <= 1);
    if(!q.size()) {
      task.discard();
      return;
    }
    task.setCallback([q, n{std::move(name)}](AppTaskArgs&) mutable {
      q.get<0>(0).at() = std::move(n);
    });
    builder.submitTask(std::move(task));
  }
}
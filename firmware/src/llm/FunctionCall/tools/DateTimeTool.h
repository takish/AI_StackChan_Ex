#ifndef _DATETIME_TOOL_H
#define _DATETIME_TOOL_H

#include "../ToolBase.h"

class GetDateTool : public ToolBase {
public:
  const char* name() const override { return "get_date"; }
  const char* schema_json() const override;
  String execute(JsonObject args) override;
};

class GetTimeTool : public ToolBase {
public:
  const char* name() const override { return "get_time"; }
  const char* schema_json() const override;
  String execute(JsonObject args) override;
};

class GetWeekTool : public ToolBase {
public:
  const char* name() const override { return "get_week"; }
  const char* schema_json() const override;
  String execute(JsonObject args) override;
};

#endif  // _DATETIME_TOOL_H

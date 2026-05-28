#ifndef _FUNCTION_CALL_H
#define _FUNCTION_CALL_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "StackchanExConfig.h"
#include "../ChatGPT/MCPClient.h"
#include "llm/LLMBase.h"
#include "ToolBase.h"

//#define USE_EXTENSION_FUNCTIONS

#define APP_DATA_PATH   "/app/AiStackChanEx/"
#define FNAME_NOTEPAD   "notepad.txt"
#define FNAME_BUS_TIMETABLE             "bus_timetable.txt"
#define FNAME_BUS_TIMETABLE_HOLIDAY     "bus_timetable_holiday.txt"
#define FNAME_BUS_TIMETABLE_SAT         "bus_timetable_sat.txt"
#define FNAME_ALARM_MP3 "alarm.mp3"

extern const String json_Functions;
extern TimerHandle_t xAlarmTimer;
extern String note;

extern bool register_wakeword_required;
extern bool wakeword_enable_required;
extern bool alarmTimerCallbacked;
extern bool alarmTimerCanceled;


class FunctionCall{
private:
    llm_param_t _param;
    LLMBase* _llm;
    MCPClient** _mcpClient;

    // Plugin-style tool registry。main.cpp の composition root から
    // register_tool() で登録される。exec_calledFunc は先にこの registry を引き、
    // ヒットしなければ既存の if-else cascade に落ちる。
    std::vector<ToolBase*> _tools;

public:
    FunctionCall(llm_param_t param, LLMBase* llm, MCPClient** mcpClient = nullptr);

    void init_func_call_settings(StackchanExConfig& system_config);
    String exec_calledFunc(const char* name, const char* args);

    // 新規 Tool を登録。ownership は呼び出し側に残る（main.cpp が保持）。
    void register_tool(ToolBase* tool);

    // json_Functions（static な配列）と登録済み tools の schema をマージした
    // 完全な functions JSON 配列文字列を返す。
    //
    // LLM 各実装（ChatGPT.cpp / GeminiLive.cpp / ChatModuleLLMFncl.cpp）は
    // 直接 json_Functions を使う代わりにこれを呼ぶ。
    String combined_functions_json() const;
    

    // Functions for Function Calling
    //
    String fn_update_memory(LLMBase* llm, const char* memory);

    #if defined(REALTIME_API)
    String set_avatar_expression(const char* expression);
    #endif

    String timer(int32_t time, const char* action);
    String timer_change(int32_t time);

    String get_date();
    String get_time();
    String get_week();

    #if defined(ARDUINO_M5STACK_CORES3)
    #if defined(ENABLE_WAKEWORD)
    String register_wakeword(void);
    String wakeword_enable(void);
    String delete_wakeword(int idx);
    #endif
    #endif

    #if defined(USE_EXTENSION_FUNCTIONS)
    String reminder(int hour, int min, const char* text);
    String ask(const char* text);
    #endif  //USE_EXTENSION_FUNCTIONS
};

#endif //_FUNCTION_CALL_H

# 故意失败应在5秒内结束。退出码1表示正常报告测试失败，异常崩溃/超时不能算通过。
# 使用独立进程接收退出码，不把主驱动回归本身设置成“失败也算成功”。
execute_process(
    COMMAND "${TOUCH_TEST_EXE}" --self-test-failure
    RESULT_VARIABLE failure_exit
    ERROR_VARIABLE failure_message
    OUTPUT_VARIABLE unexpected_output
    TIMEOUT 5)

if(NOT "${failure_exit}" STREQUAL "1")
    message(FATAL_ERROR "Failure reporting must exit 1 without waiting: ${failure_exit}")
endif()

# 除了非零状态，还必须给出文件/行号和故意失败的条件，避免空日志或错误程序蒙混通过。
if(NOT failure_message MATCHES "FAIL .*touch_driver_test\\.c:[0-9]+:" OR
   NOT failure_message MATCHES "intentional failure reporting check")
    message(FATAL_ERROR "Missing failure location/condition: ${failure_message}${unexpected_output}")
endif()
message(STATUS "PASS: failed check exits 1 promptly and reports its location/condition")

include_guard()

add_subdirectory(Benchmarks)
add_subdirectory(TestRunner)
add_subdirectory(DemoRunner)
add_subdirectory(Reason)
add_subdirectory(EngineInPluginDemo)

add_custom_target("AllExamples")
add_dependencies("AllExamples"
    Benchmarks
    TestRunner
    DemoRunner
    Reason
    EngineInPluginDemo_common)

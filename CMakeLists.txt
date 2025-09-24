include_guard()

cmake_minimum_required (VERSION 3.15...3.20)

# Add a build option for enabling examples
option(TE_ADD_EXAMPLES "Add the example targets" ON)

file(READ VERSION.md CURRENT_VERSION)
project(tracktion
        VERSION ${CURRENT_VERSION}
        DESCRIPTION
        "High level data model and set of classes for building \
        sequence based audio applications."
        HOMEPAGE_URL "https://github.com/Tracktion/tracktion_engine"
)

if (JUCE_CPM_DEVELOP)
    include(cmake/CPM.cmake)
    CPMAddPackage("gh:juce-framework/JUCE#develop")
else()
    add_subdirectory(modules/juce)
endif()

add_subdirectory(modules)

if (TE_ADD_EXAMPLES)
    enable_testing()
    add_subdirectory(examples)
endif()

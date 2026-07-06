set(TSECONV_NAME tse_complex_converter)

include_directories(BEFORE ${CMAKE_CURRENT_LIST_DIR}/compat)

file(GLOB TSECONV_CPP_SOURCES ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
file(GLOB TSECONV_CPP_INCS ${CMAKE_CURRENT_LIST_DIR}/*.h)

file(GLOB TSECONV_INC_GUI ${NATID_SDK_INC}/gui/*.h)
file(GLOB TSECONV_INC_TD ${NATID_SDK_INC}/td/*.h)
file(GLOB TSECONV_INC_CNT ${NATID_SDK_INC}/cnt/*.h)
file(GLOB TSECONV_INC_MU ${NATID_SDK_INC}/mu/*.h)
file(GLOB TSECONV_INC_MEM ${NATID_SDK_INC}/mem/*.h)
file(GLOB TSECONV_INC_FO ${NATID_SDK_INC}/fo/*.h)
file(GLOB TSECONV_INC_SC ${NATID_SDK_INC}/sc/*.h)
file(GLOB TSECONV_INC_SYST ${NATID_SDK_INC}/syst/*.h)
file(GLOB TSECONV_INC_DENSE ${NATID_SDK_INC}/dense/*.h)
file(GLOB TSECONV_INC_SPARSE ${NATID_SDK_INC}/sparse/*.h)
file(GLOB TSECONV_INC_THREAD ${NATID_SDK_INC}/thread/*.h)
file(GLOB_RECURSE TSECONV_COMPAT_INCS ${CMAKE_CURRENT_LIST_DIR}/compat/*.h)

add_library(${TSECONV_NAME} SHARED
    ${TSECONV_CPP_SOURCES}
    ${TSECONV_CPP_INCS}
    ${TSECONV_INC_GUI}
    ${TSECONV_INC_TD}
    ${TSECONV_INC_CNT}
    ${TSECONV_INC_MU}
    ${TSECONV_INC_MEM}
    ${TSECONV_INC_FO}
    ${TSECONV_INC_SC}
    ${TSECONV_INC_SYST}
    ${TSECONV_INC_DENSE}
    ${TSECONV_INC_SPARSE}
    ${TSECONV_INC_THREAD}
    ${TSECONV_COMPAT_INCS}
)

source_group("src" FILES ${TSECONV_CPP_SOURCES})
source_group("inc\\project" FILES ${TSECONV_CPP_INCS})
source_group("inc\\gui" FILES ${TSECONV_INC_GUI})
source_group("inc\\td" FILES ${TSECONV_INC_TD})
source_group("inc\\cnt" FILES ${TSECONV_INC_CNT})
source_group("inc\\mu" FILES ${TSECONV_INC_MU})
source_group("inc\\mem" FILES ${TSECONV_INC_MEM})
source_group("inc\\fo" FILES ${TSECONV_INC_FO})
source_group("inc\\sc" FILES ${TSECONV_INC_SC})
source_group("inc\\syst" FILES ${TSECONV_INC_SYST})
source_group("inc\\dense" FILES ${TSECONV_INC_DENSE})
source_group("inc\\sparse" FILES ${TSECONV_INC_SPARSE})
source_group("inc\\thread" FILES ${TSECONV_INC_THREAD})
source_group("inc\\compat" FILES ${TSECONV_COMPAT_INCS})

target_link_libraries(${TSECONV_NAME}
    debug ${MU_LIB_DEBUG} optimized ${MU_LIB_RELEASE}
    debug ${MATRIX_LIB_DEBUG} optimized ${MATRIX_LIB_RELEASE}
    debug ${NATGUI_LIB_DEBUG} optimized ${NATGUI_LIB_RELEASE}
)

if (WIN32)
    target_link_libraries(${TSECONV_NAME} Comdlg32.lib)
endif()

target_compile_definitions(${TSECONV_NAME} PUBLIC PLUGIN_EXPORTS)
target_compile_features(${TSECONV_NAME} PRIVATE cxx_std_17)

if (COMMAND setIDEPropertiesForLib)
    setIDEPropertiesForLib(${TSECONV_NAME})
else()
    setPlatformDLLPath(${TSECONV_NAME})
endif()

if (WIN32)
    set(DTWIN_PLUGIN_DIR "$ENV{USERPROFILE}/ba.natID/dTwin/plugins" CACHE PATH "dTwin user plugin folder")
    add_custom_command(TARGET ${TSECONV_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DTWIN_PLUGIN_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${TSECONV_NAME}>" "${DTWIN_PLUGIN_DIR}/$<TARGET_FILE_NAME:${TSECONV_NAME}>"
        COMMENT "Copying ${TSECONV_NAME} plugin to ${DTWIN_PLUGIN_DIR}"
    )
endif()

# Merge static libraries into a big static lib. The resulting library 
# should not not have dependencies on other static libraries.
# We use it in MySQL to merge mysys,dbug,vio etc into mysqlclient
FUNCTION(TOKU_GET_DEPENDEND_OS_LIBS target result)
  SET(deps ${${target}_LIB_DEPENDS})
  IF(deps)
   FOREACH(lib ${deps})
    # Filter out keywords for used for debug vs optimized builds
    IF(NOT lib MATCHES "general" AND NOT lib MATCHES "debug" AND NOT lib MATCHES "optimized")
      GET_TARGET_PROPERTY(lib_location ${lib} LOCATION)
      IF(NOT lib_location)
        SET(ret ${ret} ${lib})
      ENDIF()
    ENDIF()
   ENDFOREACH()
  ENDIF()
  SET(${result} ${ret} PARENT_SCOPE)
ENDFUNCTION(TOKU_GET_DEPENDEND_OS_LIBS)

MACRO(TOKU_MERGE_STATIC_LIBS TARGET OUTPUT_NAME LIBS_TO_MERGE)
  # To produce a library we need at least one source file.
  # It is created by ADD_CUSTOM_COMMAND below and will helps 
  # also help to track dependencies.
  SET(SOURCE_FILE ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_depends.cc)
  ADD_LIBRARY(${TARGET} STATIC ${SOURCE_FILE})
  SET_TARGET_PROPERTIES(${TARGET} PROPERTIES OUTPUT_NAME ${OUTPUT_NAME})

  SET(OSLIBS)
  FOREACH(LIB ${LIBS_TO_MERGE})
    GET_TARGET_PROPERTY(LIB_LOCATION ${LIB} LOCATION)
    GET_TARGET_PROPERTY(LIB_TYPE ${LIB} TYPE)
    IF(NOT LIB_LOCATION)
       # 3rd party library like libz.so. Make sure that everything
       # that links to our library links to this one as well.
       LIST(APPEND OSLIBS ${LIB})
    ELSE()
      # This is a target in current project
      # (can be a static or shared lib)
      IF(LIB_TYPE STREQUAL "STATIC_LIBRARY")
        SET(STATIC_LIBS ${STATIC_LIBS} ${LIB_LOCATION})
        ADD_DEPENDENCIES(${TARGET} ${LIB})
        # Extract dependend OS libraries
        TOKU_GET_DEPENDEND_OS_LIBS(${LIB} LIB_OSLIBS)
        LIST(APPEND OSLIBS ${LIB_OSLIBS})
      ELSE()
        # This is a shared library our static lib depends on.
        LIST(APPEND OSLIBS ${LIB})
      ENDIF()
    ENDIF()
  ENDFOREACH()
  IF(OSLIBS)
    #LIST(REMOVE_DUPLICATES OSLIBS)
    TARGET_LINK_LIBRARIES(${TARGET} ${OSLIBS})
  ENDIF()

  # Make the generated dummy source file depended on all static input
  # libs. If input lib changes,the source file is touched
  # which causes the desired effect (relink).
  ADD_CUSTOM_COMMAND( 
    OUTPUT  ${SOURCE_FILE}
    COMMAND ${CMAKE_COMMAND}  -E touch ${SOURCE_FILE}
    DEPENDS ${STATIC_LIBS})

  IF(MSVC)
    # To merge libs, just pass them to lib.exe command line.
    SET(LINKER_EXTRA_FLAGS "")
    FOREACH(LIB ${STATIC_LIBS})
      SET(LINKER_EXTRA_FLAGS "${LINKER_EXTRA_FLAGS} ${LIB}")
    ENDFOREACH()
    SET_TARGET_PROPERTIES(${TARGET} PROPERTIES STATIC_LIBRARY_FLAGS 
      "${LINKER_EXTRA_FLAGS}")
  ELSE()
    GET_TARGET_PROPERTY(TARGET_LOCATION ${TARGET} LOCATION)  
    IF(APPLE)
      # Use OSX's libtool to merge archives (ihandles universal 
      # binaries properly)
      ADD_CUSTOM_COMMAND(TARGET ${TARGET} POST_BUILD
        COMMAND rm ${TARGET_LOCATION}
        COMMAND /usr/bin/libtool -static -o ${TARGET_LOCATION} 
        ${STATIC_LIBS}
      )  
    ELSE()
      # Generic Unix, Cygwin or MinGW. In post-build step, call
      # script, that extracts objects from archives with "ar x" 
      # and repacks them with "ar r"
      SET(TARGET ${TARGET})
      CONFIGURE_FILE(
        ${TOKU_CMAKE_SCRIPT_DIR}/merge_archives_unix.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/merge_archives_${TARGET}.cmake 
        @ONLY
      )
      ADD_CUSTOM_COMMAND(TARGET ${TARGET} POST_BUILD
        COMMAND rm ${TARGET_LOCATION}
        COMMAND ${CMAKE_COMMAND} -P 
        ${CMAKE_CURRENT_BINARY_DIR}/merge_archives_${TARGET}.cmake
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/merge_archives_${TARGET}.cmake"
      )
    ENDIF()
  ENDIF()
ENDMACRO(TOKU_MERGE_STATIC_LIBS)

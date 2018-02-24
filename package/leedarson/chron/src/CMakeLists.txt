cmake_minimum_required(VERSION 2.6)

PROJECT(chron C)
#ADD_DEFINITIONS(-Os -ggdb -Wall -Werror --std=gnu99 -Wmissing-declarations) 
ADD_DEFINITIONS(-Os -ggdb -Wall --std=gnu99 -Wmissing-declarations) 

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

SET(SOURCES main.c chron.c cron.c ubus.c database.c)

SET(LIBS sqlite3 uci ubox ubus blobmsg_json json-c)

IF(DEBUG)
  ADD_DEFINITIONS(-DDEBUG -g3)
ENDIF()

ADD_EXECUTABLE(chron ${SOURCES})

TARGET_LINK_LIBRARIES(chron ${LIBS})

INSTALL(TARGETS chron RUNTIME DESTINATION usr/bin)


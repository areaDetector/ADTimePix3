TOP = ..

include $(TOP)/configure/CONFIG

USR_CPPFLAGS += -I$(TOP)/test/support
USR_CPPFLAGS += -I$(TOP)/tpx3App/src
USR_CPPFLAGS += -std=c++17

vpath %.cpp $(TOP)/test/support
vpath %.cpp $(TOP)/tpx3App/src

TESTPROD_HOST += servalProtocolFixtureTest
servalProtocolFixtureTest_SRCS += servalProtocolFixtureTest.cpp
servalProtocolFixtureTest_SRCS += FakeServalHttpServer.cpp
servalProtocolFixtureTest_SRCS += FakeServalTcpServer.cpp
servalProtocolFixtureTest_SRCS += bpc_file_io.cpp
servalProtocolFixtureTest_SRCS += network_client.cpp
servalProtocolFixtureTest_SRCS += serval_config.cpp
servalProtocolFixtureTest_SRCS += serval_http_client.cpp
servalProtocolFixtureTest_SRCS += serval_stream_validation.cpp
servalProtocolFixtureTest_LIBS += Com
servalProtocolFixtureTest_SYS_LIBS += cpr curl z

TESTS += servalProtocolFixtureTest
TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES

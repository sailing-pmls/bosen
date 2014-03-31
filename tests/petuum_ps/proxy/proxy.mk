
TESTS_PROXY_DY_LIBS = -Wl,-Bdynamic -pthread -ltbb
TESTS_PROXY_ST_LIBS = -Wl,-Bstatic -lzmq -lboost_thread -lboost_program_options 
TESTS_PROXY_ST_LIBS += -lboost_system -lglog -lgflags -lrt -lnsl -luuid -ltcmalloc -lunwind

TESTS_COMM_SRC = $(SRC)/petuum_ps/comm_handler
TESTS_PROXY_TESTS = $(TESTS_DIR)/petuum_ps/proxy

TESTS_COMM_SRC_CPP = $(TESTS_COMM_SRC)/*.cpp
TESTS_COMM_SRC_HPP = $(TESTS_COMM_SRC)/*.hpp

tests_proxy_client: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		$(TESTS_PROXY_TESTS)/basic/client.cpp \
		$(SRC)/petuum_ps/util/utils.cpp \
		$(SRC)/petuum_ps/util/utils.hpp
	$(CXX) $(INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
		$(SRC)/petuum_ps/consistency/*.cpp \
	$(TESTS_PROXY_TESTS)/basic/client.cpp $(SRC)/petuum_ps/util/utils.cpp \
	$(TESTS_PROXY_ST_LIBS) $(TESTS_PROXY_DY_LIBS) -o $(TESTS_BIN)/$@

tests_proxy_server: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		$(TESTS_PROXY_TESTS)/basic/server.cpp \
		$(SRC)/petuum_ps/util/utils.cpp \
		$(SRC)/petuum_ps/util/utils.hpp 
	$(CXX) $(INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
		$(SRC)/petuum_ps/consistency/*.cpp \
	$(TESTS_PROXY_TESTS)/basic/server.cpp $(SRC)/petuum_ps/util/utils.cpp \
	$(TESTS_PROXY_ST_LIBS) $(TESTS_PROXY_DY_LIBS) -o $(TESTS_BIN)/$@

tests_proxy_server_run: tests_proxy_server
	GLOG_v=0 GLOG_logtostderr=false \
	LD_LIBRARY_PATH=$(THIRD_PARTY_INSTALLED)/lib $(TESTS_BIN)/$<

tests_proxy_client_run: tests_proxy_client
	GLOG_v=0 GLOG_logtostderr=false \
	LD_LIBRARY_PATH=$(THIRD_PARTY_INSTALLED)/lib $(TESTS_BIN)/$<

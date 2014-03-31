
TESTS_COMM_DY_LIBS = -Wl,-Bdynamic -pthread
TESTS_COMM_ST_LIBS = -Wl,-Bstatic -lzmq -lboost_thread -lboost_program_options 
TESTS_COMM_ST_LIBS += -lboost_system -lglog -lgflags -lrt -lnsl -luuid -ltcmalloc -lunwind

#unwind is needed by tcmalloc

TESTS_COMM_SRC = $(SRC)/petuum_ps/comm_handler
TESTS_COMM_TESTS = $(TESTS_DIR)/petuum_ps/comm_handler

TESTS_COMM_SRC_CPP = $(TESTS_COMM_SRC)/*.cpp
TESTS_COMM_SRC_HPP = $(TESTS_COMM_SRC)/*.hpp

TESTS_COMM_INCFLAGS = -I$(TESTS_COMM_SRC) -I$(SRC)

tests_comm_all: tests_comm_basic tests_comm_chain tests_comm_benchmark \
	tests_comm_async tests_comm_pubsub tests_comm_multipart_message

tests_comm_basic: tests_comm_server tests_comm_client
tests_comm_chain: tests_comm_chain_server tests_comm_chain_client tests_comm_chain_bridge
tests_comm_benchmark: tests_comm_benserver tests_comm_benclient
tests_comm_multithread: tests_comm_mserver tests_comm_mclient
tests_comm_async: tests_comm_aserver tests_comm_aclient
tests_comm_pubsub: tests_comm_pubserver tests_comm_subclient
tests_comm_multipart_message: tests_comm_mpserver tests_comm_mpclient
tests_comm_ts: tests_comm_tsserver tests_comm_tsclient
tests_comm_km: tests_comm_keyserver tests_comm_keyclient
tests_comm_ct1: tests_comm_ct1server tests_comm_ct1client

tests_comm_server: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/basic/server.cpp  
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/basic/server.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_client: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/basic/client.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/basic/client.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_chain_server: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/chain/server.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/chain/server.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_chain_client: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/chain/client.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/chain/client.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_chain_bridge: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/chain/bridge.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/chain/bridge.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_aserver: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/async/server.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/async/server.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_aclient: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/async/client.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/async/client.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_mserver: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/multi_thread/server.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/multi_thread/server.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_mclient: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/multi_thread/client.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/multi_thread/client.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_pubserver: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/pubsub/server.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/pubsub/server.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_subclient: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/pubsub/client.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/pubsub/client.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_benserver: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/benchmark/server.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/benchmark/server.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_benclient: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/benchmark/client.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/benchmark/client.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_mpserver: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/multipart_message/server.cpp  
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/multipart_message/server.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_mpclient: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/multipart_message/client.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/multipart_message/client.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_tsserver: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/thread_specific/server.cpp  
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/thread_specific/server.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_tsclient: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/thread_specific/client.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/thread_specific/client.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_keyserver: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/keyed_message/server.cpp  
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/keyed_message/server.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_keyclient: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/keyed_message/client.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/keyed_message/client.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_ct1server: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/complex_test1/server.cpp  
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/complex_test1/server.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

tests_comm_ct1client: mk_tests_bin $(TESTS_COMM_SRC_CPP) $(TESTS_COMM_SRC_HPP) \
		   $(TESTS_COMM_TESTS)/complex_test1/client.cpp
	$(CXX) $(INCFLAGS) $(TESTS_COMM_INCFLAGS) $(CPPFLAGS) $(TESTS_COMM_SRC_CPP) \
	$(TESTS_COMM_TESTS)/complex_test1/client.cpp $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	-o $(TESTS_BIN)/$@

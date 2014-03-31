

UTIL_SRC_HPP = $(PS_DIR)/util/mem_block.hpp 
UTIL_SRC_CPP = $(PS_DIR)/util/vector_clock.cpp

SERVER_SRC_HPP = $(PS_DIR)/server/server_threads.hpp $(PS_DIR)/server/server.hpp
SERVER_SRC_CPP = $(PS_DIR)/server/server_threads.cpp $(PS_DIR)/server/server.cpp

STORAGE_SRC_HPP = $(PS_DIR)/storage/process_storage.hpp $(PS_DIR)/storage/clock_lru.hpp \
	$(PS_DIR)/storage/client_row.hpp $(PS_DIR)/storage/dense_row.hpp \
	$(PS_DIR)/storage/dense_row.hpp
STORAGE_SRC_CPP = $(PS_DIR)/storage/process_storage.cpp $(PS_DIR)/storage/client_row.cpp \
	$(PS_DIR)/storage/clock_lru.cpp

OPLOG_SRC_HPP = $(PS_DIR)/oplog/oplog.hpp $(PS_DIR)/oplog/oplog_partition.hpp
OPLOG_SRC_CPP = $(PS_DIR)/oplog/oplog_partition.cpp 

COMM_BUS_SRC_HPP = $(PS_DIR)/comm_bus/comm_bus.hpp $(PS_DIR)/comm_bus/zmq_util.hpp
COMM_BUS_SRC_CPP = $(PS_DIR)/comm_bus/comm_bus.cpp $(PS_DIR)/comm_bus/zmq_util.cpp

PS_INCLUDE_HPP = $(PS_DIR)/include/table_group.hpp $(PS_DIR)/include/table.hpp \
	$(PS_DIR)/include/abstract_row.hpp

THREADS_SRC_HPP = $(PS_DIR)/thread/bg_workers.hpp $(PS_DIR)/thread/context.hpp \
	$(PS_DIR)/thread/ps_msgs.hpp $(PS_DIR)/thread/msg_tracker.hpp
THREADS_SRC_CPP = $(PS_DIR)/thread/bg_workers.cpp $(PS_DIR)/thread/context.cpp \
	$(PS_DIR)/thread/msg_tracker.cpp

tests_bg: $(TESTS)/petuum_ps/thread/bg_workers_tests.cpp $(UTIL_SRC_HPP) \
	$(STORAGE_SRC_HPP) $(STORAGE_SRC_CPP) $(OPLOG_SRC_HPP) $(OPLOG_SRC_CPP) \
	$(COMM_BUS_SRC_HPP) $(COMM_BUS_SRC_CPP) $(THREADS_SRC_HPP) \
	$(THREADS_SRC_CPP) $(SERVER_SRC_HPP) $(SERVER_SRC_CPP)
	$(CXX) $(INCFLAGS) -I$(SRC) $(CXXFLAGS) \
	$(TESTS)/petuum_ps/thread/bg_workers_tests.cpp $(STORAGE_SRC_CPP) \
	$(OPLOG_SRC_CPP) $(COMM_BUS_SRC_CPP) $(THREADS_SRC_CPP) $(SERVER_SRC_CPP) \
	$(UTIL_SRC_CPP) $(LDFLAGS) -o $(BIN)/$@

tests_bg_workers: $(THREADS_SRC_HPP) $(THREADS_SRC_CPP)
	$(CXX) $(INCFLAGS) $(CXXFLAGS) bg_workers_tests.cpp \
	$(THREADS_SRC_CPP) $(LDFLAGS) -o $(BIN)/$@
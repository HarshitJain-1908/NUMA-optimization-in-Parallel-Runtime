include $(HCLIB_ROOT)/include/hclib.mak

TARGETS := trace_and_replay_iterative_averaging iterative_averaging

all: $(TARGETS)

trace_and_replay_iterative_averaging: iterative_averaging.cpp
	$(CXX) -O3 -DUSE_TRACE_AND_REPLAY -DVERIFY $(PROJECT_CXXFLAGS) $(PROJECT_LDFLAGS) -o $@ $^  $(PROJECT_LDLIBS) -g

iterative_averaging: iterative_averaging.cpp
	$(CXX) -O3 $(PROJECT_CXXFLAGS) $(PROJECT_LDFLAGS) -o $@ $^  $(PROJECT_LDLIBS)

clean:
	rm -rf *.o $(TARGETS)

include $(HCLIB_ROOT)/include/hclib.mak

TARGETS := iterative_averaging

all: clean $(TARGETS) clean-obj

%: %.cpp
	$(CXX) -O3 $(PROJECT_CXXFLAGS) $(PROJECT_LDFLAGS) -o $@ $^  $(PROJECT_LDLIBS)

clean-obj:
	rm -rf *.o

clean:
	rm -rf *.o $(TARGETS)

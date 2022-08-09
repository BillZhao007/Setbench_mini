CFLAGS =
LDFLAGS =
CC=g++

CFLAGS += -DMEMORY_STATS=if\(1\) -DMEMORY_STATS2=if\(0\)
CFLAGS += -DMAX_THREADS_POW2=256
CFLAGS += -DCPU_FREQ_GHZ=2.2
CFLAGS += -g -std=c++17 -O3 -lstdc++fs
# CFLAGS += -fopenmp
CFLAGS += $(xargs)
CFLAGS += -Wno-format
CFLAGS += -DNDEBUG
CFLAGS += -DNO_CLEANUP_AFTER_WORKLOAD

### if you do not have PAPI, comment out these two lines
CFLAGS += -DUSE_PAPI -I ${PAPI_HOME}/include -L ${PAPI_HOME}/lib -Wall
LDFLAGS += -lpapi -I ${PAPI_HOME}/include -L ${PAPI_HOME}/lib -Wall
#
#LDFLAGS += -lnuma

workload=TPCC
data_structure_name=bronson_pext_bst_occ

.SUFFIXES: .o .cpp .h
bindir=../../bin
odir=$(bindir)/OBJS_$(data_structure_name)
SRC_DIRS = ./
CFLAGS += -I../../ $(patsubst %,-I%,$(SRC_DIRS)) `find ../../common -type d | sed s/^/-I/`

CFLAGS += -DNOGRAPHITE=1 #-DWORKLOAD=$(workload)
CFLAGS += -DALIGNED_ALLOCATIONS
CFLAGS += -fno-omit-frame-pointer
LDFLAGS += $(CFLAGS)
LDFLAGS += -L. -L../../lib -g
LDFLAGS += -lpthread

CPPS = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)*.cpp))
OBJS = $(foreach obj, $(CPPS:.cpp=.o), $(odir)/$(obj))
dir_guard=@mkdir -p $(@D)

all: $(bindir)/rundb_$(data_structure_name)

$(bindir)/rundb_$(data_structure_name): $(OBJS)
	$(dir_guard)
	$(CC) -o $@ $^ $(LDFLAGS)

$(odir)/%.o: %.cpp
	$(dir_guard)
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	@rm -f $(bindir)/rundb_$(data_structure_name) 2>&1 >/dev/null
	@rm -r -f $(odir) 2>&1 >/dev/null

clean_objs:
	@rm -r -f $(odir) 2>&1 >/dev/null

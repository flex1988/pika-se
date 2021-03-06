build_dir := build

obj := $(build_dir)/server.o $(build_dir)/connection.o $(build_dir)/port.o $(build_dir)/command.o $(build_dir)/db.o $(build_dir)/kv.o $(build_dir)/list.o $(build_dir)/list_meta.o $(build_dir)/set.o $(build_dir)/set_meta.o $(build_dir)/hash.o $(build_dir)/meta_base.o $(build_dir)/aof.o $(build_dir)/request.o $(build_dir)/response.o $(build_dir)/btree.o


tests = \
	$(build_dir)/db_test \
	$(build_dir)/meta_test	\
	$(build_dir)/list_test
	

benchmarks = \
	$(build_dir)/list_benchmark

.PHONY: all

all: blink $(obj) $(test)

CC := g++

CFLAGS := -g -lmuduo_net_cpp11 -lmuduo_base_cpp11 -lrocksdb -lz -lbz2 -lgtest -lpthread -Iinclude -std=c++11 -Wall

blink: $(obj)
	$(CC) -o build/blink src/blink.cc $(wildcard build/*.o) $(CFLAGS)

check: $(tests)
	@rm -rf /tmp/db
	@mkdir /tmp/db
	@for t in $(notdir $(tests)); do echo "***** Running $$t"; $(build_dir)/$$t || exit 1; done

benchmark: $(benchmarks)

$(build_dir)/%_test: test/%_test.cc $(obj)
	@test -d build || mkdir -p build
	$(CC) $< $(obj) $(CFLAGS) -o $@

$(build_dir)/%_benchmark: benchmark/%_benchmark.cc $(obj)
	@test -d build || mkdir -p build
	$(CC) $< $(obj) $(CFLAGS) -lbenchmark -lcpp_redis -ltacopie -o $@ 

$(build_dir)/%.o: src/%.cc
	@test -d build || mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@rm -rf build

tclean:
	@rm build/*test

# GUROBI_DIR ?= /home/uvxiao/env/gurobi 
# JSONHPP_DIR ?= xxx
EXTERNEL_DIR = $(wildcard ./externel)

PYTHON = python3
CC 		= 	gcc
CPP 	= 	g++
CARGS = 	-m64 -g

# INC 	=		$(GUROBI_DIR)/include $(JSONHPP_DIR)
# CPPLIB   = -L${GUROBI_DIR}/lib -lgurobi_c++ -lgurobi100

INC 	= $(EXTERNEL_DIR)
CPPLIB = -L$(EXTERNEL_DIR) -lgurobi_c++ -lgurobi100

MSKD = ./build/mskd_cc
EXEC = $(MSKD)

$(MSKD): mskd.cc mskd.h
	mkdir -p ./build
	$(CPP) $(CARGS) -g -o $@ $< -I$(INC) $(CPPLIB) -lm 

compile: $(EXEC)  
run: compile 
	mkdir -p ./logs
	$(EXEC) 2>&1 >> stdout.log

data: datagen.py
	mkdir -p ./json/random
	rm -f ./json/random/*
	$(PYTHON) datagen.py
	
test: data run.py $(EXEC)
	mkdir -p ./result
	rm -f ./result/*
	$(PYTHON) run.py
	
analyze: analyzer.py
	mkdir -p ./production
	rm -f ./production/*
	$(PYTHON) analyzer.py

clean:
	rm -rf build/* logs/* *log result/*
GUROBI_DIR ?= /home/uvxiao/env/gurobi 

CC 		= 	gcc
CPP 	= 	g++
CARGS = 	-m64 -g
INC 	=		$(GUROBI_DIR)/include
CPPLIB   = -L${GUROBI_DIR}/lib -lgurobi_c++ -lgurobi100

MSKD = ./build/mskd_cc
EXEC = $(MSKD)

$(MSKD): mskd.cc mskd.h
	mkdir -p ./build
	$(CPP) $(CARGS) -o $@ $< -I$(INC) $(CPPLIB) -lm

run: $(EXEC) 
	$(EXEC)
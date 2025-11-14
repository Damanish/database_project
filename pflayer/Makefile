#PUBLICDIR= /usr0/cs564/public/project
SRC= buf.c hash.c pf.c
OBJ= buf.o hash.o pf.o
RHF_OBJ= rhf.o
HDR = pftypes.h pf.h 

pflayer.o: $(OBJ)
	ld -r -o pflayer.o $(OBJ)

tests: testhash testpf testpf_stats testpf_workload testrhf

testpf: testpf.o pflayer.o
	cc -o testpf testpf.o pflayer.o

testhash: testhash.o pflayer.o
	cc -o testhash testhash.o pflayer.o

# NEW: Rule for building testpf_stats
testpf_stats: testpf_stats.o pflayer.o
	cc -o testpf_stats testpf_stats.o pflayer.o

testpf_workload: testpf_workload.o pflayer.o
	cc -o testpf_workload testpf_workload.o pflayer.o

testrhf: testrhf.o $(RHF_OBJ) pflayer.o
	cc -o testrhf testrhf.o $(RHF_OBJ) pflayer.o

$(OBJ): $(HDR)

testhash.o: $(HDR)

testpf.o: $(HDR)

# NEW: Rule for testpf_stats.o
testpf_stats.o: $(HDR)

testpf_workload.o: $(HDR)

testrhf.o: $(HDR)

lint: 
	lint $(SRC)

install: pflayer.o 

clean:
	del /f *.o testpf.exe testhash.exe testpf_stats.exe


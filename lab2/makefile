build: my_print.o
	@echo "going to compile main.cpp and link"
	@g++ -o main.out -m32 main.cpp my_print.o
	@echo "compile main.cpp and link finished"

my_print.o:
	@echo "going to compile my_print.asm"
	@nasm -felf32 -o my_print.o my_print.asm
	@echo "compile my_print.asm finished"

run: build
	@./main.out

clean:
	@rm -f *.o
	@rm -f *.out

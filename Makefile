CC = gcc

all: simpsh

simpsh:
			$(CC) simpsh.c -o simpsh


dist: lab1-LukeMoore.tar.gz
sources = README Makefile simpsh.c Lab1c.pdf
lab1-LukeMoore.tar.gz: $(sources)
			tar cf - $(sources) | gzip -9 >$@
clean:
			rm -f *.o *.tmp $(tests) simpsh lab1-LukeMoore.tar.gz

check:	simpsh
	@touch file1
	@touch file2
	@touch fileErr

	@# Test regular I/O with a command
	@echo "world\nhello" > file1
	@echo ""
	./simpsh --rdonly file1 --wronly file2 --command 0 1 2 sort 2> fileErr
	if [ $$? -eq 0 ] ; then \
		echo "This test case was successful"; \
	fi
	@echo ""
	@# Test input from --rdonly and output to stdout with verbose enabled
	./simpsh --rdonly file1 --verbose --command 0 1 2 cat
	if [ $$? -eq 0 ] ; then \
		echo "This test case was successful"; \
	fi
	@echo ""
	@# Test with no --rdonly or --wronly options
	./simpsh --command 0 1 2 pwd
	@echo "Should show working directory above this line!!!"
	@echo ""
	@# Test with multiple commands and I/O received and --verbose after 1 command
	./simpsh --command 0 1 2 pwd --verbose --rdonly file1 --wronly file2 \
	--wronly fileErr --command 0 1 2 sort
	@echo "Should see only one verbose case being printed!!!!"
	@echo ""

	@# Testing rdwr option with wait option
	./simpsh --rdwr file1 --rdwr file2 --wronly fileErr --command 0 1 2 cat --wait
	@echo "Should see exit status info above"
	@echo ""
	@# Testing pipe option
	./simpsh --pipe --rdonly file1 --wronly fileErr --wronly file2 --command 2 1 3 cat --command 0 4 3 cat
	@cat file2
	@echo "Should see output text above!"

	@echo ""
	@# Testing pipe option with append and other file options
	./simpsh --pipe --rdwr file1 --append --creat --wronly file2 --creat --wronly fileErr --command 2 1 4 cat --command 0 3 4 cat
	@cat file2
	@echo "Should see output twice above due to --append option!!"

	@rm file1
	@rm file2
	@rm fileErr

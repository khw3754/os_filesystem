hw2 : disk.o hw1.o hw2.o testcase.o
	gcc -o hw2 disk.o hw1.o hw2.o testcase.o

disk.o : disk.c
	gcc -c -o disk.o disk.c

hw1.o : hw1.c
	gcc -c -o hw1.o hw1.c

hw2.o : hw2.c
	gcc -c -o hw2.o hw2.c

testcase.o : testcase.c
	gcc -c -o testcase.o testcase.c

clean :
	rm disk.o hw1.o hw2.o testcase.o MY_DISK hw2

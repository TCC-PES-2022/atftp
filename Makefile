all:
	@echo "Building tftp client library..."
	gcc -c -Wall -Werror -fPIC tftp.c tftp_io.c logger.c options.c tftp_def.c tftp_file.c tftp_mtftp.c -g -ggdb
	gcc -shared -o libtftp.so tftp.o tftp_io.o logger.o options.o tftp_def.o tftp_file.o tftp_mtftp.o -g -ggdb

	@echo "Done!"

clean:
	rm -f *.o *.so
	@echo "Done!"

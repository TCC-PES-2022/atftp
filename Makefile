all:
	@echo "Building tftp client library..."
	gcc -c -Wall -Werror -fPIC tftp.c tftp_io.c logger.c options.c tftp_def.c tftp_file.c tftp_mtftp.c -g -ggdb
	gcc -shared -o libtftp.so tftp.o tftp_io.o logger.o options.o tftp_def.o tftp_file.o tftp_mtftp.o -g -ggdb

	@echo "Building tftp server library..."
	gcc -c -Wall -Werror -fPIC tftpd.c logger.c options.c stats.c tftp_io.c tftp_def.c tftpd_file.c tftpd_list.c \
								tftpd_mcast.c tftpd_pcre.c tftpd_mtftp.c -g -ggdb
	gcc -shared -o libtftpd.so tftpd.o logger.o options.o stats.o tftp_io.o tftp_def.o tftpd_file.o tftpd_list.o \
                              	tftpd_mcast.o tftpd_pcre.o tftpd_mtftp.o -g -ggdb

	@echo "Done!"

clean:
	rm -f *.o *.so
	@echo "Done!"

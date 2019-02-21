#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

/*
Function : TCP 소켓으로 지정한 서버에 접속한다.
Format : Int SB_ConnectTcp2 (char *IP_Address, int Socket_No, int Wait_Msec, Int Tx_Size, int Rx_Size);
Parameter : IP_Address : 접속할 서버의 IP 주소 문자열
			Socket_No : 접속할 서버의 소켓번호
			Wait_Msec : 접속 대기시간 (밀리초 단위)
			Tx_Size : 소켓의 Tx 버퍼 사이즈 (K bytes 단위)
			Rx_Size : 소켓의 Rx 버퍼 사이즈 (K bytes 단위)
Returns : 	-1 ~ N : 연결된 소켓의 핸들번호
			-1 : 연결 실패
			N : 연결된 핸들번호
Notice : 접속요청 즉시 연결이 안되면 Wait_Msec 로 지정한 시간만큼
		 재접속을 대기한 후 리턴한다.
		 Tx,Rx_Size 는 소켓의 버퍼 사이즈로 최소 1 ~ 64 까지 설정 가능하다.
		 1 보다 작은 값을 입력하면 디폴트 4 kbytes 로 처리되며,
		 64 보다 큰 수를 입력하면 64 kbytes 로 처리된다.
*/
int SB_ConnectTcp2 (char *IP_address, unsigned int socket_no, int wait_msec, int Tx_Size, int Rx_Size)
{
	int sockfd;
	struct sockaddr_in addr;
	long arg;
	int result;
	fd_set myset;
	struct timeval tv;
	int valopt;
	socklen_t lon;

	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Error creating socket (%d %s)\n", errno, strerror(errno));
		return -1;
	}

/*
	lon = sizeof(valopt);
	if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &valopt, &lon) < 0) {
		fprintf(stderr, "Error in getsockopt(SO_RCVBUF) %d - %s\n", errno, strerror(errno));
		goto ERROR;
	}
	//printf("receive buffer size = %d\n", valopt);

	if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &valopt, &lon) < 0) {
		fprintf(stderr, "Error in getsockopt(SO_SNDBUF) %d - %s\n", errno, strerror(errno));
		goto ERROR;
	}
	//printf("send buffer size = %d\n", valopt);

	if (getsockopt(sockfd, SOL_SOCKET, SO_RCVLOWAT, &valopt, &lon) < 0) {
		fprintf(stderr, "Error in getsockopt(SO_RCVLOWAT) %d - %s\n", errno, strerror(errno));
		goto ERROR;
	}
	//printf("rcv low watermark = %d\n", valopt);

	if (getsockopt(sockfd, SOL_SOCKET, SO_SNDLOWAT, &valopt, &lon) < 0) {
		fprintf(stderr, "Error in getsockopt(SO_SNDLOWAT) %d - %s\n", errno, strerror(errno));
		goto ERROR;
	}
	//printf("snd low watermark = %d\n", valopt);
*/

	valopt = Rx_Size * 1024 / 2;
	//valopt = 512*2;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &valopt, sizeof(valopt)) < 0) {
		fprintf(stderr, "Error in setsockopt(SO_RCVBUF) %d - %s\n", errno, strerror(errno));
		goto ERROR;
	}

	lon = sizeof(valopt);
	if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &valopt, &lon) < 0) {
		fprintf(stderr, "Error in getsockopt(SO_RCVBUF) %d - %s\n", errno, strerror(errno));
		goto ERROR;
	}
	//printf("modified receive buffer size = %d, %d\n", valopt, Rx_Size);

	valopt = Tx_Size * 1024 / 2;
	//valopt = 512*2;
	if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &valopt, sizeof(valopt)) < 0) {
		fprintf(stderr, "Error in setsockopt(SO_SNDBUF) %d - %s\n", errno, strerror(errno));
		goto ERROR;
	}

	lon = sizeof(valopt);
	if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &valopt, &lon) < 0) {
		fprintf(stderr, "Error in getsockopt(SO_SNDBUF) %d - %s\n", errno, strerror(errno));
		goto ERROR;
	}
	//printf("modified send buffer size = %d, %d\n", valopt, Tx_Size);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(socket_no);
	addr.sin_addr.s_addr = inet_addr(IP_address);

	// Set non-blocking
	if( (arg = fcntl(sockfd, F_GETFL, NULL)) < 0) {
		fprintf(stderr, "Error fcntl(..., F_GETFL) (%s)\n", strerror(errno));
		goto ERROR;
	}
	arg |= O_NONBLOCK;
	if( fcntl(sockfd, F_SETFL, arg) < 0) {
		fprintf(stderr, "Error fcntl(..., F_SETFL) (%s)\n", strerror(errno));
		goto ERROR;
	}

	// Trying to connect with timeout
	result = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
	if (result < 0) {
		if (errno == EINPROGRESS) {
			//fprintf(stderr, "EINPROGRESS in connect() - selecting\n");
			do {
				tv.tv_sec = wait_msec / 1000;
				tv.tv_usec = (wait_msec % 1000) * 1000;
				FD_ZERO(&myset);
				FD_SET(sockfd, &myset);
				result = select(sockfd + 1, NULL, &myset, NULL, &tv);
				if (result < 0 && errno != EINTR) {
					fprintf(stderr, "Error connecting %d - %s\n", errno, strerror(errno));
					goto ERROR;
				}
				else if (result > 0) {
					// Socket selected for write
					lon = sizeof(int);
					if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon) < 0) {
						fprintf(stderr, "Error in getsockopt() %d - %s\n", errno, strerror(errno));
						goto ERROR;
					}
					// Check the value returned...
					if (valopt) {
						fprintf(stderr, "Error in delayed connection() %d - %s\n", valopt, strerror(valopt)); 
						goto ERROR;
					}
					break;
				}
				else {
					//fprintf(stderr, "Timeout in select() - Cancelling!\n");
					goto ERROR;
				}
			} while (1);
		}
		else {
			fprintf(stderr, "Error connecting %d - %s\n", errno, strerror(errno));
			goto ERROR;
		}
	}

	return sockfd;

ERROR:
	close(sockfd);
	return -1;
}


void main(void)
{
	int fd;
	int len;
	
	fd = SB_ConnectTcp2("192.168.1.1", 4000, 100, 8, 8);  // timeout = 100msec, tx/rx buf size = 8KB
	if (fd > 1) {
		len = strlen("MESSAGE");  			///GOOD_PACKET_RESET_STR);
		write(fd, "MESSAGE", len);    ///GOOD_PACKET_RESET_STR);
		shutdown(fd, SHUT_RDWR);
		close(fd);
	}
}
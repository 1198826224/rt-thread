#include <dfs_posix.h>
#include <lwip/sockets.h>

#include <finsh.h>

#define TFTP_PORT	69
/* opcode */
#define TFTP_RRQ			1 	/* read request */
#define TFTP_WRQ			2	/* write request */
#define TFTP_DATA			3	/* data */
#define TFTP_ACK			4	/* ACK */
#define TFTP_ERROR			5	/* error */

rt_uint8_t tftp_buffer[512 + 4];
/* tftp client */
void tftp_get(const char* host, const char* filename)
{
	int fd, sock_fd;
	struct sockaddr_in tftp_addr, from_addr;
	rt_uint32_t length;
	socklen_t fromlen;

	/* make local file name */
	getcwd((char*)tftp_buffer, sizeof(tftp_buffer));
	strcat((char*)tftp_buffer, "/");
	strcat((char*)tftp_buffer, filename);

	/* open local file for write */
	fd = open((char*)tftp_buffer, O_RDWR, 0);
	if (fd < 0)
	{
		rt_kprintf("can't open local filename\n");
		return;
	}

	/* connect to tftp server */
    inet_aton(host, (struct in_addr*)&(tftp_addr.sin_addr));
    tftp_addr.sin_family = AF_INET;
    tftp_addr.sin_port = htons(TFTP_PORT);
    
    sock_fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if (sock_fd < 0)
	{
	    close(fd);
	    rt_kprintf("can't create a socket\n");
	    return ;
	}
	
	/* make tftp request */
	tftp_buffer[0] = 0;			/* opcode */
	tftp_buffer[1] = TFTP_RRQ; 	/* RRQ */
	length = rt_sprintf((char *)&tftp_buffer[2], "%s", filename) + 2;
	tftp_buffer[length] = 0; length ++;
	length += rt_sprintf((char*)&tftp_buffer[length], "%s", "octet");
	tftp_buffer[length] = 0; length ++;

	fromlen = sizeof(struct sockaddr_in);
	
	/* send request */	
	lwip_sendto(sock_fd, tftp_buffer, length, 0, 
		(struct sockaddr *)&tftp_addr, fromlen);
	
	do
	{
		length = lwip_recvfrom(sock_fd, tftp_buffer, sizeof(tftp_buffer), 0, 
			(struct sockaddr *)&from_addr, &fromlen);
		
		if (length > 0)
		{
			write(fd, &tftp_buffer[4], length - 4);
			rt_kprintf("#");

			/* make ACK */			
			tftp_buffer[0] = 0; tftp_buffer[1] = TFTP_ACK; /* opcode */
			/* send ACK */
			lwip_sendto(sock_fd, tftp_buffer, 4, 0, 
				(struct sockaddr *)&from_addr, fromlen);
		}
	} while (length != 516);
	
	close(fd);
	lwip_close(sock_fd);
}
FINSH_FUNCTION_EXPORT(tftp_get, get file from tftp server);

void tftp_put(const char* host, const char* filename)
{
	int fd, sock_fd;
	struct sockaddr_in tftp_addr, from_addr;
	rt_uint32_t length, block_number = 0;
	socklen_t fromlen;

	/* make local file name */
	getcwd((char*)tftp_buffer, sizeof(tftp_buffer));
	strcat((char*)tftp_buffer, "/");
	strcat((char*)tftp_buffer, filename);

	/* open local file for write */
	fd = open((char*)tftp_buffer, O_RDONLY, 0);
	if (fd < 0)
	{
		rt_kprintf("can't open local filename\n");
		return;
	}

	/* connect to tftp server */
    inet_aton(host, (struct in_addr*)&(tftp_addr.sin_addr));
    tftp_addr.sin_family = AF_INET;
    tftp_addr.sin_port = htons(TFTP_PORT);
    tftp_addr.sin_addr.s_addr = htonl(tftp_addr.sin_addr.s_addr);
    
    sock_fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if (sock_fd < 0)
	{
	    close(fd);
	    rt_kprintf("can't create a socket\n");
	    return ;
	}
	
	/* make tftp request */
	tftp_buffer[0] = 0;			/* opcode */
	tftp_buffer[1] = TFTP_WRQ; 	/* WRQ */
	length = rt_sprintf((char *)&tftp_buffer[2], "%s", filename) + 2;
	tftp_buffer[length] = 0; length ++;
	length += rt_sprintf((char*)&tftp_buffer[length], "%s", "octet");
	tftp_buffer[length] = 0; length ++;

	fromlen = sizeof(struct sockaddr_in);
	
	/* send request */	
	lwip_sendto(sock_fd, tftp_buffer, length, 0, 
		(struct sockaddr *)&tftp_addr, fromlen);

	/* wait ACK 0 */	
	length = lwip_recvfrom(sock_fd, tftp_buffer, sizeof(tftp_buffer), 0, 
		(struct sockaddr *)&from_addr, &fromlen);
	if (!(tftp_buffer[0] == 0 &&
		tftp_buffer[1] == TFTP_ACK &&
		tftp_buffer[2] == 0 &&
		tftp_buffer[3] == 0))
	{
		rt_kprintf("tftp server error\n");
		close(fd);
		return;
	}

	do
	{
		rt_uint16_t *ptr;
		ptr = (rt_uint16_t*)&tftp_buffer[0];

		length = read(fd, &tftp_buffer[4], 512);
		if (length > 0)
		{
			/* make opcode and block number */
			*ptr = TFTP_DATA; *(ptr + 1) = block_number;

			lwip_sendto(sock_fd, tftp_buffer, length + 4, 0, 
				(struct sockaddr *)&tftp_addr, fromlen);
		}
		else break; /* no data yet */

		/* receive ack */
		length = lwip_recvfrom(sock_fd, tftp_buffer, sizeof(tftp_buffer), 0, 
			(struct sockaddr *)&from_addr, &fromlen);
		if (length > 0)
		{
			if (*ptr == TFTP_ACK && *(ptr + 1) == block_number)
			{
				block_number ++;
			}
			else 
			{
				rt_kprintf("server respondes with an error\n");
				break;
			}
		}
	} while (length != 516);
	
	close(fd);
	lwip_close(sock_fd);
}
FINSH_FUNCTION_EXPORT(tftp_put, put file to tftp server);

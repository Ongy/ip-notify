#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <syslog.h>

#include <linux/rtnetlink.h>
#include <linux/netlink.h>

#define SCRIPT "/usr/lib/ip-notify"

#define OUTPUT(x, ...) "%s(%d): " x, __FILE__, __LINE__, __VA_ARGS__

#define DEBUG(x, ...) do { \
	syslog(LOG_DEBUG, OUTPUT(x, __VA_ARGS__)); \
	} while (0)

#define INFO(x, ...) do { \
	syslog(LOG_INFO, OUTPUT(x, __VA_ARGS__)); \
	} while (0)

#define ERR(x, ...) do { \
	syslog(LOG_ERR, OUTPUT(x, __VA_ARGS__)); \
	exit(-1); \
	} while(0)

#ifndef SOL_NETLINK
#define SOL_NETLINK 270 /*This is not accessible from userspace*/
#endif /*SOL_NETLINK*/

#define NEXT_NLATTR(nlattr) \
	(struct nlattr *)(((uint8_t *)(nlattr)) + NLA_ALIGN((nlattr)->nla_len))

#define for_each_nl(buffer, nlattr, len) \
	for ((nlattr) = (struct nlattr *)(buffer); \
	    (uint8_t *)(nlattr) - (buffer) < (len); \
	    (nlattr) = NEXT_NLATTR(nlattr))


static int get_socket()
{
	int fd;
	struct sockaddr_nl sa = { AF_NETLINK, 0, 0, 0 };

	fd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	if (fd < 0) {
		ERR("%s", strerror(errno));
	}

	if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		ERR("%s", strerror(errno));
	}
	return fd;
}

static void join_multicast(int fd, int mcgroup)
{
	if (setsockopt(fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
		       &mcgroup, sizeof(mcgroup)) < 0) {
		ERR("%s", strerror(errno));
	}
}


static ssize_t recv_msg(int fd, uint8_t *buffer, size_t maxlen)
{
	struct sockaddr_nl sa;
	struct iovec iov = { buffer, maxlen };
	struct msghdr msg = { &sa, sizeof(sa), &iov, 1, NULL, 0, 0 };

	return recvmsg(fd, &msg, 0);
}

struct addrchange {
	int action;
	int family;
	uint8_t *addr;
	char *dev;
};

static void spawn_action(const struct addrchange *change)
{
	char ip[INET6_ADDRSTRLEN];
	char buffer[512];

	snprintf(buffer, sizeof(buffer), "%s %s %s %s",
		 SCRIPT,
		 change->action == RTM_NEWADDR ? "add" : "delete",
		 inet_ntop(change->family, change->addr, ip, sizeof(ip)),
		 change->dev);

	DEBUG("spawning: %s", buffer);

	system(buffer);
}

static void handle_attr(const struct nlattr *nlattr, struct addrchange *change)
{
	switch(nlattr->nla_type) {
	case IFA_ADDRESS:
		change->addr = (uint8_t *)(nlattr + 1);
		return;
	case IFA_LABEL:
		change->dev = (char*)(nlattr + 1);
		return;
	default:
		return;
	}
}

static void handle_message(const struct nlmsghdr *message, ssize_t len)
{
	uint8_t *payload;
	ssize_t length;
	struct nlattr *nlattr;
	struct ifaddrmsg *ifaddr;

	struct addrchange change;

	if (!NLMSG_OK(message, len)) {
		ERR("%s", "Got damaged message\n");
	}
	if (message->nlmsg_type != RTM_NEWADDR
	   && message->nlmsg_type != RTM_DELADDR) {
		INFO("%s", "Got unexpected message type");
		return;
	}

	ifaddr = (struct ifaddrmsg *)NLMSG_DATA(message);

	change.family = ifaddr->ifa_family;
	change.action = message->nlmsg_type;

	payload = (uint8_t *)(ifaddr + 1);
	length = NLMSG_PAYLOAD(message, sizeof(struct ifaddrmsg));

	for_each_nl(payload, nlattr, length) {
		handle_attr(nlattr, &change);
	}

	spawn_action(&change);
}

static void mainloop(int fd)
{
	uint8_t buffer[4096];
	ssize_t ret;

	while (1) {
		if ((ret = recv_msg(fd,buffer,sizeof(buffer))) < 0) {
			ERR("%s", strerror(errno));
		}
		handle_message((struct nlmsghdr *)buffer, ret);
	}
}

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	int fd;

	openlog(argv[0], 0, LOG_USER);

	fd = get_socket();
	join_multicast(fd, RTNLGRP_IPV4_IFADDR);

	mainloop(fd);

	close(fd);

	return 0;
}

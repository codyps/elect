/* Vote
 * - request validation number from CLA
 * - calculate random identification number
 * - send vote,validation,rident to CTF
 * - verify vote was counted
 */
#include "warn.h"
#include "tcp.h"
#include "proto.h"

#include <unistd.h>

static int get_vnum(char *cla_addr, char *cla_port,
		char *name, char *pass, valid_num_t *vn)
{
	struct addrinfo *ai_cla;
	int r = tcp_resolve_as_client(cla_addr, cla_port, &ai_cla);
	if (r) {
		w_prt("resolve of cla [%s]:%s failed: %s\n",
				cla_addr, cla_port,
				tcp_resolve_strerror(r));
		return 2;
	}

	int cla_fd = tcp_connect(ai_cla);
	if (cla_fd == -1) {
		w_prt("connect to cla [%s]:%s failed: %s\n",
				cla_addr, cla_port,
				tcp_resolve_strerror(r));
		return 4;
	}

	r = cla_get_vnum(cla_fd, name, pass, vn);
	if (r) {
		w_prt("cla get vnum failed: %d\n", r);
		return 5;
	}

	close(cla_fd);
	freeaddrinfo(ai_cla);
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 8) {
		w_prt("usage: %s <cla addr> <cla port> <ctf addr> <ctf port> <name> <pass> <vote>\n",
			argc?argv[0]:"vote");
		return 1;
	}

	valid_num_t vn;
	int r = get_vnum(argv[1], argv[2], argv[5], argv[6], &vn);
	if (r) {
		w_prt("failed to get validation number\n");
		return 3;
	}

	struct addrinfo *ai_ctf;
	r = tcp_resolve_as_client(argv[3], argv[4], &ai_ctf);
	if (r) {
		w_prt("resolve of ctf [%s]:%s failed: %s\n",
				argv[3], argv[4],
				tcp_resolve_strerror(r));
		return 3;
	}

	int ctf_fd = tcp_connect(ai_ctf);
	if (ctf_fd == -1) {
		w_prt("connect to ctf [%s]:%s failed: %s\n",
				argv[3], argv[4],
				tcp_resolve_strerror(r));
		return 6;
	}

	ident_num_t in;
	ident_num_init(&in);
	r = ctf_send_vote(ctf_fd, argv[7], &vn, &in);
	if (r) {
		w_prt("ctf send vote failed: %d\n", r);
		return 7;
	}

	close(ctf_fd);

	freeaddrinfo(ai_ctf);

	return 0;
}

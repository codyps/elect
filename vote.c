/* Vote
 * - request validation number from CLA
 * - calculate random identification number
 * - send vote,validation,rident to CTF
 * - verify vote was counted
 */
#include "warn.h"
#include "tcp.h"
#include "proto.h"

int main(int argc, char *argv[])
{
	if (argc != 5) {
		w_prt("usage: %s <cla addr> <cla port> <ctf addr> <ctf port> <id> <vote>\n",
			argc?argv[0]:"vote");
		return 1;
	}

	struct addrinfo *ai_cla;
	int r = tcp_resolve_as_client(argv[1], argv[2], &ai_cla);
	if (r) {
		w_prt("resolve of cla [%s]:%s failed: %s\n",
				argv[1], argv[2],
				tcp_resolve_strerror(r));
		return 2;
	}

	struct addrinfo *ai_ctf;
	r = tcp_resolve_as_client(argv[3], argv[4], &ai_ctf);
	if (r) {
		w_prt("resolve of ctf [%s]:%s failed: %s\n",
				argv[3], argv[4],
				tcp_resolve_strerror(r));
		return 3;
	}

	int cla_fd = tcp_connect(ai_cla);
	if (cla_fd == -1) {
		w_prt("connect to cla [%s]:%s failed: %s\n",
				argv[1], argv[2],
				tcp_resolve_strerror(r));
		return 4;
	}

	valid_num_t vn;
	r = cla_get_vnum(cla_fd, argv[5], &vn);
	if (r) {
		w_prt("cla get vnum failed: %d\n", r);
		return 5;
	}

	close(cla_fd);

	int ctf_fd = tcp_connect(ai_ctf);
	if (ctf_fd == -1) {
		w_prt("connect to ctf [%s]:%s failed: %s\n",
				argv[3], argv[4],
				tcp_resolve_strerror(r));
		return 6;
	}

	ident_num_t in;
	ident_num_init(&in);

	r = ctf_send_vote(ctf_fd, argv[6], &vn, &in);
	if (r) {
		w_prt("ctf send vote failed: %d\n", r);
		return 7;
	}

	close(ctf_fd);

	freeaddrinfo(ai_ctf);
	freeaddrinfo(ai_cla);

	return 0;
}

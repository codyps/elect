/* Vote
 * - request validation number from CLA
 * - calculate random identification number
 * - send vote,validation,rident to CTF
 * - verify vote was counted
 */
#include "warn.h"
#include "tcp.h"

int main(int argc, char *argv[])
{
	if (argc != 5) {
		w_prt("usage: %s <cla addr> <cla port> <ctf addr> <ctf port>\n",
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


	freeaddrinfo(ai_ctf);
	freeaddrinfo(ai_cla);

	return 0;
}

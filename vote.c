/* Vote
 * - request validation number from CLA
 * - calculate random identification number
 * - send vote,validation,rident to CTF
 * - verify vote was counted
 */
#include "warn.h"
#include "tcp.h"
#include "proto.h"

#include <time.h>
#include <unistd.h>

static int get_vnum(char *cla_addr, char *cla_port,
		char *name, char *pass, valid_num_t *vn)
{
	int cla_fd = tcpw_resolve_and_connect("cla", cla_addr, cla_port);
	if (cla_fd < 0) {
		return 1;
	}

	int r = cla_get_vnum(cla_fd, name, strlen(name), pass, strlen(pass), vn);
	if (r) {
		w_prt("cla get vnum failed: %d\n", r);
		return 2;
	}

	close(cla_fd);
	return 0;
}

int main(int argc, char *argv[])
{
	srand(time(NULL));

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

	printf("validation number: ");
	valid_num_print(&vn, stdout);
	putchar('\n');

	int ctf_fd = tcpw_resolve_and_connect("ctf", argv[3], argv[4]);
	if (ctf_fd == -1) {
		return 6;
	}

	ident_num_t in;
	ident_num_init(&in);
	r = ctf_send_vote(ctf_fd, argv[7], strlen(argv[7]), &vn, &in);
	if (r) {
		w_prt("ctf send vote failed: %d\n", r);
		return 7;
	}

	printf("ident number: ");
	ident_num_print(&in, stdout);
	putchar('\n');

	close(ctf_fd);
	return 0;
}

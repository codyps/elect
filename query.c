/* Query - list who voted and who did not.
 */
#include "warn.h"
#include "tcp.h"


int main(int argc, char *argv[])
{
	if (argc != 3) {
		w_prt("usage: %s <cla addr> <cla port>\n",
				argc?argv[0]:"query");
		return 1;
	}

	struct addrinfo *cla_ai;
	int r = tcpw_resolve_as_client("cla", argv[1], argv[2], &cla_ai);
	if (r)
		return 2;





	return 0;
}

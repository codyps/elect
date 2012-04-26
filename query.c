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

	int fd = tcpw_resolve_and_connect("cla", argv[1], argv[2]);
	if (fd < 0)
		return 2;




	return 0;
}

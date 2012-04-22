/* Vote
 * - request validation number from CLA
 * - calculate random identification number
 * - send vote,validation,rident to CTF
 * - verify vote was counted
 */
#include "warn.h"

int main(int argc, char *argv[])
{
	if (argc != 5) {
		w_prt("usage: %s <cla addr> <cla port> <ctf addr> <ctf port>\n",
			argc?argv[0]:"vote");
		return 1;
	}
	return 0;
}

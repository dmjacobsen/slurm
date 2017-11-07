/******************************************************************************
** Author: Douglas Jacobsen <dmjacobsen@lbl.gov>
******************************************************************************/

#define _GNU_SOURCE /* for getline */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/time.h>

#include "slurm/slurm.h"
#include "src/common/macros.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"

static int stop_running = 0;
static pthread_mutex_t down_node_lock;
static int *down_node;
static size_t n_down_node;
static size_t down_node_sz;

typedef enum event_type {
	EVENT_INVALID = 0,
	EVENT_NODE_FAILED,
	EVENT_NODE_UNAVAILABLE
} event_type_t;

int getnid(const char *cname, int dim) {
        int cabinet, row, chassis, slot, node;
        int nodes_per_slot = 4;
        int nodes_per_chassis = nodes_per_slot * 16;
        int nodes_per_cabinet = nodes_per_chassis * 3;
        int nodes_per_row = nodes_per_cabinet * dim;
        sscanf(cname, "c%d-%dc%ds%dn%d",
                &cabinet, &row, &chassis, &slot, &node);
        return cabinet * nodes_per_cabinet + row * nodes_per_row +
                chassis * nodes_per_chassis + slot * nodes_per_slot + node;
}

char *getnidlist() {
	char *ret = NULL;
	size_t idx = 0;
	int last_nid = 0;
	int in_range = 0;
	snprintf(ret, ret_sz, "nid[");
	for (idx = 0; idx < n_nodes_down; idx++) {
		int curr_nid = nodes_down[idx];
		if (last_nid == 0) {
			xstrfmtcat(&ret, "%05d", curr_nid);
		} else if (curr_nid - last_nid > 1) {
			if (in_range) {
				xstrfmtcat(&ret, "-%05d", last_nid);
			}
			xstrfmtcat(&ret, ",%05d", curr_nid);
			in_range = 0;
		} else if (idx == n_nodes_down - 1) {
			xstrfmtcat(&ret, "-%05d", curr_nid);
		} else {
			in_range = 1;
		}
		last_nid = curr_nid;
	}
	xstrfmtcat(&ret, "]");
	return ret;
}

int _mark_nodes_down() {
	/* locks are assumed to be held */
	int rc = 0;
	update_node_msg_t *update_msg = xmalloc(sizeof(update_node_msg_t));

	slurm_init_update_node_msg(update_msg);

	update_msg->node_names = getnidlist();
	update_msg->node_state = NODE_STATE_NO_RESPOND;

	info("smw_xtconsumer_slurm_helperd: setting %s to NORESP", nodelist);

	rc = slurm_update_node(update_msg);

	slurm_free_update_node_msg(update_msg);
	return rc;

}

void *process_data(void *arg) {
	while (1) {
		slurm_mutex_lock(&down_node_lock);
		if (n_down_nodes > 0) {
			_mark_nodes_down();
			n_down_nodes = 0;
		}
		slurm_mutex_unlock(&down_node_lock);
		usleep(2000000);

	}
}

event_type_t _parse_event(const char *input) {
	if (strstr(input, "ec_node_failed") != NULL)
		return EVENT_NODE_FAILED;
	if (strstr(input, "ec_node_unavailable") != NULL)
		return EVENT_NODE_UNAVAILABLE;
	return EVENT_INVALID;
}

int _cmp_nid(void *a, void *b, void *arg) {
	int ai = * (int *) a;
	int bi = * (int *) b;
	return ai - bi;
}

void _send_failed_nodes(const char *nodelist) {
	char *search = nodelist;
	char *svptr = NULL;
	char *ptr = NULL;
	int nid = 0;
	slurm_mutex_lock(&down_node_lock);
	while ((ptr = strtok_r(search, " ", &svptr)) != NULL) {
		while (*ptr == ':')
			ptr++;
		nid = getnid(ptr, SYSTEM_CABINETS_PER_ROW);
		while (n_down_node + 1 >= down_node_sz) {
			size_t alloc_quantity = (down_node_sz + 4) * 2;
			size_t alloc_size = sizeof(int) * alloc_quantity;
			down_node = xrealloc(down_node, alloc_size);
			down_node_sz = alloc_quantity;
		}
		down_node[n_down_node++] = nid;
	}
	qsort_r(down_node, n_down_node, sizeof(int), _cmp_nid, NULL);
	slurm_mutex_unlock(&down_node_lock);
}

/*
2017-05-16 07:17:12|2017-05-16 07:17:12|0x40008063 - ec_node_failed|src=:1:s0|::c4-2c0s2n0 ::c4-2c0s2n2 ::c4-2c0s2n3
2017-05-16 07:17:12|2017-05-16 07:17:12|0x400020e8 - ec_node_unavailable|src=:1:s0|::c4-2c0s2n2
2017-05-16 08:11:01|2017-05-16 08:11:01|0x400020e8 - ec_node_unavailable|src=:1:s0|::c4-2c0s2n0 ::c4-2c0s2n1 ::c4-2c0s2n2 ::c4-2c0s2n3
*/
void *xtconsumer_listen(void *arg) {
	FILE *fp = NULL;
	const char *command =
		"/opt/cray/hss/default/bin/xtconsumer -b ec_node_unavailable ec_node_failed";
	char *line = NULL;
	size_t line_sz = 0;

	fp = popen(command, "r");
	if (fp == NULL) {
		fprintf(stderr, "Failed to open xtconsumer: %s (%d)\n",
			strerror(errno), errno);
		return NULL;
	}

	/* xtconsumer seems to flush out its stdout on newline (typical)
	 * so reading line-by-line seems to be functional for this need
	 * setting line buffering on our half of the pipe is probably useless
	 */
	setlinebuf(fp);
	while (!feof(fp) && !ferror(fp)) {
		/* using _GNU_SOURCE getline because xtconsumer linelength may be HUGE, and I
		 * prefer not to screw around with fgets and extra buffer content joining for
		 * now
		 */
		ssize_t bytes = getline(&line, &line_sz, fp);
		event_type_t event = EVENT_INVALID;
		char *node_list = NULL;
		char *search = NULL;
		char *ptr = NULL;
		char *svptr = NULL;
		int token_idx = 0;
		if (bytes <= 0)
			break;

		search = line;
		while ((ptr = strtok_r(search, "|", &svptr)) != NULL) {
			search = NULL;
			if (token_idx == 2)
				event = _parse_event(ptr);
			if (token_idx == 4)
				node_list = xstrdup(ptr);

			token_idx++;
		}
		if (event_type == EVENT_INVALID)
			goto cleanup_continue;

		if (event_type == EVENT_NODE_FAILED ||
		    event_type == EVENT_NODE_UNAVAILABLE) {
			_send_failed_nodes(node_list);
		}

cleanup_continue:
		if (node_list) {
			xfree(node_list);
			node_list = NULL;
		}
		continue;
cleanup_break:
		if (node_list)
			xfree(node_list);
		break;
	}
	pclose(fp);
	if (line != NULL) {
		/* line is allocated by getline(), so should be freed here */
		free(line);
	}
	return NULL;

}

int main(int argc, char **argv) {
	pthread_t xtc_thread, processing_thread;
	slurm_mutex_init(&down_node_lock);

	pthread_create(&processing_thread, NULL, &process_data, NULL);

	for ( ; ; ) {
		pthread_create(&xtc_thread, NULL, &xtconsumer_listen, NULL);
		pthread_join(xtc_thread, NULL);
	}

	pthread_join(processing_thread, NULL);
	_mutex_destroy(&down_node_lock);
	return 0;
}

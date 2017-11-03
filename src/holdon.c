/*
 * Copyright (c) 2017-now, Taras Korenko
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define EVENTBUFFER_SZ 4096

static const uint32_t eventsSelector =
	IN_MODIFY | IN_CLOSE_WRITE | IN_DELETE_SELF | IN_MOVE_SELF;

#if 0
#define MK_MASK_NAME_PAIR(VAL)	{ VAL, #VAL }
static const struct {
	uint32_t mask;
	const char * name;
} inotify_mask_names[] = {
	MK_MASK_NAME_PAIR( IN_ACCESS		),
	MK_MASK_NAME_PAIR( IN_MODIFY		),
	MK_MASK_NAME_PAIR( IN_ATTRIB		),
	MK_MASK_NAME_PAIR( IN_CLOSE_WRITE	),
	MK_MASK_NAME_PAIR( IN_CLOSE_NOWRITE	),
	MK_MASK_NAME_PAIR( IN_OPEN		),
	MK_MASK_NAME_PAIR( IN_MOVED_FROM	),
	MK_MASK_NAME_PAIR( IN_MOVED_TO		),
	MK_MASK_NAME_PAIR( IN_CREATE		),
	MK_MASK_NAME_PAIR( IN_DELETE		),
	MK_MASK_NAME_PAIR( IN_DELETE_SELF	),
	MK_MASK_NAME_PAIR( IN_MOVE_SELF		),
};
#endif

struct tracked_object_s {
	int watch_descr;
	const char * name;
};

static unsigned int tracked_objects_qty = 0u;
static struct tracked_object_s * tracked_objects = NULL;
static int inotify_fd = -1;

static int
is_file(const char * name)
{
	struct stat sb;

	if (-1 == stat(name, &sb)) {
		warn("\"%s\"", name);

		return -1;
	}

	return ( (sb.st_mode & S_IFREG) ? 1 : 0);
}

#if 0
static const char *
decrypt_inotify_mask(uint32_t val)
{
	const unsigned masknames_qty =
	    sizeof(inotify_mask_names) / sizeof(inotify_mask_names[0]);
	static char outstr[256];
	char tmpstr[32];
	unsigned i;

	outstr[0] = '\0';
	for (i = 0; i < masknames_qty; ++i) {
		if (val & inotify_mask_names[i].mask) {
			sprintf(tmpstr, "%s ", inotify_mask_names[i].name);
			strcat(outstr, tmpstr);
		}
	}

	return outstr;
}
#endif

static const char *
find_name_by_watch_descr(int wd)
{
	const char * name = "<no match>";
	int i;

	for (i = 0; i < tracked_objects_qty; ++i) {
		if (tracked_objects[i].watch_descr == wd) {
			name = tracked_objects[i].name;
			break;
		}
	}

	return name;
}

static void
watchlist_init(int items)
{
	int i;

	inotify_fd = inotify_init();
	if (inotify_fd < 0) {
		err(1, "inotify_init() failed");
		/* NOTREACHED */
	}

	tracked_objects = malloc(sizeof(struct tracked_object_s) * items);
	if (NULL == tracked_objects) {
		err(1, "malloc() failed");
		/* NOTREACHED */
	}

	for (i = 0; i < items; ++i) {
		tracked_objects[i].watch_descr = -1;
		tracked_objects[i].name = NULL;
	}
}

static int
watchlist_add_files(int items, char * names[])
{
	int i;
	unsigned object_idx = 0u;

	for (i = 0; i < items; ++i) {
		int wd;
		const char * name = names[i];

		if (is_file(name) <= 0)
			continue;
		
		if ( 0 != access(name, R_OK) ) {
			warn("cannot read \"%s\"", name);
			continue;
		}

		wd = inotify_add_watch(inotify_fd, name, eventsSelector);
		if (wd < 0) {
			warn("cannot watch \"%s\"", name);
			continue;
		}

		tracked_objects[object_idx].watch_descr = wd;
		tracked_objects[object_idx].name = name;

		++object_idx;
	}

	tracked_objects_qty = object_idx;

	return object_idx;
}

static const char *
watchlist_wait_for_event(void)
{
	char eventbuffer[EVENTBUFFER_SZ];
	struct inotify_event * pe = (struct inotify_event *)&eventbuffer[0];
	ssize_t rs;
	const char * fired_file = "";

	rs = read(inotify_fd, eventbuffer, (size_t)EVENTBUFFER_SZ);
	if (rs > 0) {
		fired_file = find_name_by_watch_descr(pe->wd);
	}

	return fired_file;
}

static void
watchlist_destroy(void)
{
	int i;

	for (i = 0; i < tracked_objects_qty; ++i) {
		inotify_rm_watch(inotify_fd, tracked_objects[i].watch_descr);
		tracked_objects[i].watch_descr = -1;
		tracked_objects[i].name = NULL;
	}

	free(tracked_objects);
	tracked_objects = NULL;

	close(inotify_fd);

	inotify_fd = -1;
}

static const char * progname = NULL;

static void
printUsage(void)
{
	fprintf(stderr, "usage:\n    %s <file1> [<file2>.. <fileN>]\n",
	    progname);
}

int
main(int argc, char * argv[])
{
	const char * fn = "";

	progname = basename(argv[0]);

	if (argc < 2) {
		printUsage();
		return 1;
	}

	--argc; ++argv;

	watchlist_init(argc);

	if (0 == watchlist_add_files(argc, argv)) {
		fprintf(stderr, "nothing to watch on...\n");
		goto out;
	}

	fn = watchlist_wait_for_event();

	printf("%s\n", fn);

out:
	watchlist_destroy();

	return 0;
}

/*-
 * Copyright (c) 2016 Allan Jude <allanjude@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/capsicum.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include <ucl.h>

#include "capsicum_helpers.h"

#define MAXOFILES	10

int debug = 0;

static void
usage()
{
	fprintf(stderr, "Usage: %s [-d] [-o output_file] file1 [file2] ...\n",
	    getprogname());
	exit(EX_USAGE);
}

/*
 * This application is a proof of concept to show that libucl can be adapted
 * to work inside of a sandbox. The first sandbox inplemented was Capsicum.
 */
int
main(int argc, char *argv[])
{
	int ret = 0, i = 0, j = 0, ch;
	int fds[MAXOFILES], output;
	ucl_object_t *root_obj = NULL;
	struct ucl_parser *parser = NULL;
	ucl_object_t *comments;
	char *result = NULL;
	struct ucl_emitter_functions *func;
	const char *outfile = NULL;
	ssize_t written;
	cap_rights_t rights_rd, rights_wr;

	cap_rights_init(&rights_rd, CAP_READ, CAP_MMAP_R, CAP_FCNTL, CAP_FSTAT);
	cap_rights_init(&rights_wr, CAP_WRITE, CAP_CREATE, CAP_FCNTL, CAP_FSTAT);

	/* Initialize parser */
	parser = ucl_parser_new(UCL_PARSER_SAVE_COMMENTS);

	/*	options	descriptor */
	static struct option longopts[] = {
	    { "debug",	optional_argument,	NULL,		'd' },
	    { "output",	required_argument,	NULL,		'o' },
	    { NULL,		0,			NULL,		0 }
	};

	while ((ch = getopt_long(argc, argv, "do:", longopts, NULL)) != -1) {
		switch (ch) {
		case 'd':
			if (optarg != NULL) {
				debug = strtol(optarg, NULL, 0);
			} else {
				debug = 1;
			}
			break;
		case 'o':
			outfile = optarg;
			break;
		default:
			err(EX_USAGE, "Error: Unexpected option: %i", ch);
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		usage();
	}

	if (argc > MAXOFILES)
		err(EX_DATAERR, "Error: The maximum number of input files is %d", MAXOFILES);

	/* Use the capsicum helpers to precache data and sandbox stdio FDs */
	caph_cache_catpages();
	if (caph_limit_stdio() == -1)
		err(EX_NOPERM, "Error: Unable to set capabilities on stdio");

	if (outfile == NULL)
		output = STDOUT_FILENO;
	else {
		output = open(outfile, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0644);
		if (output == -1)
			err(EX_CANTCREAT, "Error: Unable to open: %s", outfile);
		if (cap_rights_limit(output, &rights_wr) < 0 && errno != ENOSYS)
			err(EX_NOPERM, "Error: Unable to set capabilities on: %s", outfile);
		if (cap_fcntls_limit(output, CAP_FCNTL_GETFL) < 0 && errno != ENOSYS)
			err(EX_NOPERM, "Error: Unable to set fcntl capabilities on: %s", outfile);
	}

	for (i = 0; i < argc; i++) {
		fds[i] = open(argv[i], O_NOFOLLOW);
		if (fds[i] == -1)
			err(EX_NOINPUT, "Error: Unable to open: %s", argv[i]);
		if (cap_rights_limit(fds[i], &rights_rd) < 0 && errno != ENOSYS)
			err(EX_NOPERM, "Error: Unable to set capabilities on: %s", argv[i]);
		if (cap_fcntls_limit(fds[i], CAP_FCNTL_GETFL) < 0 && errno != ENOSYS)
			err(EX_NOPERM, "Error: Unable to set fcntl capabilities on: %s", argv[i]);
	}

	if (cap_enter() < 0 && errno != ENOSYS)
		err(EX_NOPERM, "Error: Unable to enter capability mode");

	for (j = 0; j < i; j++) {
		if (debug)
			fprintf(stderr, "DEBUG: Reading fd %d\n", j);
		ret |= ucl_parser_add_fd_priority(parser, fds[j], j);
		if (ucl_parser_get_error(parser))
			errx(EX_DATAERR, "Error: Unable to parse input: %s: %s",
			     argv[j], ucl_parser_get_error(parser));
	}

	root_obj = ucl_parser_get_object(parser);
	if (ucl_parser_get_error(parser))
		errx(EX_DATAERR, "Error: Unable to parse: %s",
		    ucl_parser_get_error(parser));

	result = NULL;
	comments = ucl_object_ref(ucl_parser_get_comments(parser));
	func = ucl_object_emit_memory_funcs((void **)&result);
	if (func != NULL) {
		if (root_obj != NULL) {
			ucl_object_emit_full(root_obj, UCL_EMIT_CONFIG, func, comments);
		}
		ucl_object_emit_funcs_free(func);
	}

	written = write(output, result, strlen(result));
	if (written != strlen(result))
		err(EX_IOERR, "Error: Failed to write to output file: %s", outfile == NULL ? "stdout" : outfile);
	if (debug)
		fprintf(stderr, "DEBUG: Wrote %lu bytes to %s\n", written,
		    outfile == NULL ? "stdout" : outfile);

	if (parser != NULL) {
		ucl_parser_free(parser);
	}
	if (root_obj != NULL) {
		ucl_object_unref(root_obj);
	}

	return(ret);
}

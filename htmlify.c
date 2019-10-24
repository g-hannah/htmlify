#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <misclib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "buffer.h"

#define QUOTE "&quot;"
#define APOS "&apos;"
#define LANGLE "&lt;"
#define RANGLE "&gt;"
#define AMPER "&amp;"

static int		fd, ofd;
static struct stat	statb;
static int		P_PARITY;

static char		PARAGRAPH_TAG[256] = "<p>";
static char		HEADING_TAG[256] = "<h1>";
static char		HEADING_CLOSE[256] = "";

static buf_t IBUF;
static buf_t OBUF;
static buf_t TMP;

void usage(int) __attribute__ ((__noreturn__));
int get_heading_close(char *, char *) __nonnull ((1,2)) __wur;
int create_page(char *fname, char *out) __nonnull ((1,2)) __wur;

static void
__attribute__((constructor)) htmify_init(void)
{
	if (buf_init(&IBUF, DEFAULT_BUFSIZE) < 0)
		goto fail;
	if (buf_init(&OBUF, DEFAULT_BUFSIZE) < 0)
		goto fail;

	return;

	fail:
	fprintf(stderr, "htmlify_init: failed to allocate memory for buffers\n");
	exit(EXIT_FAILURE);
}

static void
__attribute__((destructor)) htmlify_fini(void)
{
	buf_destroy(&IBUF);
	buf_destroy(&OBUF);

	return;
}

int
main(int argc, char *argv[])
{
	static char		c;

	while ((c = getopt(argc, argv, "p:H:h")) != -1)
	{
		switch(c)
		{
			case(0x70):
			assert(strlen(optarg) < 256);
			strncpy(PARAGRAPH_TAG, optarg, 256);
			break;
			case(0x48):
			assert(strlen(optarg) < 256);
			strncpy(HEADING_TAG, optarg, 256);
			break;
			case(0x68):
			usage(EXIT_SUCCESS);
			break;
			default:
			usage(EXIT_FAILURE);
		}
	}

	if (get_heading_close(HEADING_TAG, HEADING_CLOSE) == -1)
	{
		fprintf(stderr, "main() > get_heading_close()\n");
		goto fail;
	}

	fprintf(stdout,
		"Paragraph tag: %s\n"
		"Heading tag: %s\n"
		"Heading close: %s\n",
		PARAGRAPH_TAG,
		HEADING_TAG,
		HEADING_CLOSE);

	if (!argv[optind] || !argv[optind+1])
		usage(EXIT_FAILURE);

	if (create_page(argv[optind], argv[optind+1]) < 0)
	{
		fprintf(stderr, "main() > create_page()");
		goto fail;
	}

	exit(EXIT_SUCCESS);

	fail:
	exit(EXIT_FAILURE);
}

int
create_page(char *fname, char *out)
{
	char *p = NULL;
	char *a = NULL;
	size_t	tsize;
	int i;
	int nlcnt;

	if (access(fname, F_OK) != 0)
	  { fprintf(stderr, "create_page(): file does not exist\n"); return(-1); }

	if (access(fname, R_OK) != 0)
	  { fprintf(stderr, "create_page(): cannot read this file\n"); return(-1); }

	memset(&statb, 0, sizeof(statb));

	if (lstat(fname, &statb) < 0)
	  { fprintf(stderr, "create_page() > lstat(): %s\n", strerror(errno)); return(-1); }

	if ((fd = open(fname, O_RDONLY)) < 0)
	  { fprintf(stderr, "create_page() > open(): %s\n", strerror(errno)); return(-1); }

	if ((ofd = open(out, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU & ~S_IXUSR)) < 0)
	  { fprintf(stderr ,"create_page() > open(): %s\n", strerror(errno)); return(-1); }
	
	tsize = statb.st_size;
	i &= ~i;
	buf_append(&OBUF, PARAGRAPH_TAG);
	P_PARITY = 1;

	buf_read_fd(fd, &IBUF, statb.st_size);
	p = IBUF.buf_head;

	while (tsize > 0)
	{
		nlcnt &= ~nlcnt;
		while (p < (IBUF.buf_tail))
		{
			if (*p == 0x22)
			{
				buf_append(&OBUF, QUOTE);

				++p;

				if (p >= IBUF.buf_tail)
					break;
			}
			else
			if (*p == 0x0a || *p == 0x0d)
			{
				if ((*p == 0x0d && *(p+2) == 0x0d) || (*p == 0x0a && *(p+1) == 0x0a)) // new para
				{
					if (nlcnt == 0) // isolated line, use <h1> tags
					{
						a = OBUF.buf_tail;

						while (1)
						{
							while (*a != 0x3c && a > (OBUF.buf_head + 1))
								--a;

							if (strncasecmp(PARAGRAPH_TAG, a, strlen(PARAGRAPH_TAG)) != 0)
							{
								--a;
								continue;
							}
							else
							{
								break;
							}
						}

						a += strlen(PARAGRAPH_TAG);

						if (buf_init(&TMP, BUF_ALIGN_SIZE((OBUF.buf_tail - a))) < 0)
						{
							fprintf(stderr, "create_page: failed to initialise temporary buffer (buf_init)\n");
							goto fail;
						}

						buf_append_ex(&TMP, a, (OBUF.buf_tail - a));
						BUF_NULL_TERMINATE(&TMP);
						a -= strlen(PARAGRAPH_TAG);

						buf_push(&OBUF, (OBUF.buf_tail - a));
						buf_append(&OBUF, HEADING_TAG);
						P_PARITY = 0;

						buf_append_ex(&OBUF, TMP.buf_head, TMP.data_len);
						buf_destroy(&TMP);

						buf_append(&OBUF, HEADING_CLOSE);
						buf_append(&OBUF, "\n");
						buf_append(&OBUF, PARAGRAPH_TAG);
						P_PARITY = 1;

						while ((*p == 0x0a || *p == 0x0d) && p < IBUF.buf_tail)
							++p;

						nlcnt &= ~nlcnt;
						if (p >= IBUF.buf_tail) // not IBUF_SIZE
							break;
					}
					else // nlcnt != 0
					{
						buf_append(&OBUF, "</p>\n");
						buf_append(&OBUF, PARAGRAPH_TAG);
						while ((*p == 0x0a || *p == 0x0d) && p < IBUF.buf_tail)
							++p;

						nlcnt &= ~nlcnt;
						if (p >= IBUF.buf_tail)
							break;
							
					}
				}
				else
				// next char not 0x0a / *(p+2) not a 0x0d
				  {
					if (*p == 0x0d)
						++p;

					if (p >= IBUF.buf_tail)
						break;

					 ++p;

					++nlcnt;

					if (p >= IBUF.buf_tail)
						break;
				  }
			  }
			else
			if (*p == 0x27)
			{
				buf_append(&OBUF, APOS);

				++p;

				if (p >= IBUF.buf_tail)
					break;
			}
			else
			if (*p == 0x26)
			{
				buf_append(&OBUF, AMPER);

				++p;

				if (p >= IBUF.buf_tail)
					break;
			}
			else
			if (*p == 0x3c)
			{
				buf_append(&OBUF, LANGLE);

				++p;

				if (p >= IBUF.buf_tail)
					break;
			}
			else
			if (*p == 0x3d)
			{
				buf_append(&OBUF, "&#61;");

				++p;

				if (p >= IBUF.buf_tail)
					break;
			}
			else
			if (*p == 0x3e)
			{
				buf_append(&OBUF, RANGLE);

				++p;

				if (p >= IBUF.buf_tail)
					break;
			}
			else
			{
				++p;

				if (p >= IBUF.buf_tail)
					break;
			}
		}

		if (P_PARITY == 1)
		{
			buf_append(&OBUF, "</p>");
		}

		buf_write_fd(ofd, &OBUF);
		tsize -= (p - IBUF.buf_head);
	}

	close(ofd);
	return(0);

	fail:
	return -1;
}

int
get_heading_close(char *TAG, char *CTAG)
{
	static char		*p = NULL, *q = NULL;

	p = TAG;
	q = CTAG;

	strncpy(q, "</h", 3);
	q += 3;

	while (! isdigit(*p) && p < (TAG + strlen(TAG)))
		++p;

	if (! isdigit(*p))
		return(-1);

	*q++ = *p++;
	strncpy(q, ">", 1);
	++q;
	*q = 0;

	return(0);
}

void
usage(int exit_type)
{
	fprintf(stderr,
		"htmlify <options> <in file> <out file>\n"
		"\n"
		" -p	specify paragraph tag, e.g., \"<p id=\"id_of_p\">\"\n"
		" -H	specify heading tag (default is <h1>)\n"
		" -h	display this information menu\n");

	exit(exit_type);
}

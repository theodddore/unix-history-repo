/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)fts.h	5.1 (Berkeley) %G%
 */

typedef struct fts {
	struct ftsent *cur;		/* current node */
	struct ftsent *child;		/* linked list of children */
	struct ftsent *savelink;	/* saved link if node had a cycle */
	struct ftsent **array;		/* sort array */
	char *path;			/* path for this descent */
	char *wd;			/* starting directory */
	int pathlen;			/* sizeof(path) */
	int nitems;			/* elements in the sort array */
	int (*compar)();		/* compare function */
#define	FTS__STOP	0x001		/* private: unrecoverable error */
#define	FTS_LOGICAL	0x002		/* user: use stat(2) */
#define	FTS_MULTIPLE	0x004		/* user: multiple args */
#define	FTS_NOCHDIR	0x008		/* user: don't use chdir(2) */
#define	FTS_NOSTAT	0x010		/* user: don't require stat info */
#define	FTS_PHYSICAL	0x020		/* user: use lstat(2) */
#define	FTS_SEEDOT	0x040		/* user: return dot and dot-dot */
	int options;			/* openfts() options */
} FTS;

typedef struct ftsent {
	struct ftsent *parent;		/* parent directory */
	struct ftsent *link;		/* next/cycle node */
	union {
		long number;		/* local numeric value */
		void *pointer;		/* local address value */
	} local;
	char *accpath;			/* path from current directory */
	char *path;			/* path from starting directory */
	short pathlen;			/* strlen(path) */
	short namelen;			/* strlen(name) */
	short level;			/* depth (-1 to N) */
#define	FTS_D		 1		/* preorder directory */
#define	FTS_DC		 2		/* directory that causes cycles */
#define	FTS_DNR		 3		/* unreadable directory */
#define	FTS_DNX		 4		/* unsearchable directory */
#define	FTS_DP		 5		/* postorder directory */
#define	FTS_ERR		 6		/* error; errno is set */
#define	FTS_F		 7		/* regular file */
#define	FTS_NS		 8		/* no stat(2) information */
#define	FTS_SL		 9		/* symbolic link */
#define	FTS_SLNONE	10		/* symbolic link without target */
#define	FTS_DEFAULT	11		/* none of the above */
	u_short	info;			/* file information */
#define	FTS_AGAIN	 1		/* user: read node again */
#define	FTS_SKIP	 2		/* user: discard node */
#define	FTS_FOLLOW	 3		/* user: follow symbolic link */
	short instr;			/* setfts() instructions */
	struct stat statb;		/* stat(2) information */
	char name[1];			/* file name */
} FTSENT;

FTS *ftsopen();
FTSENT *ftschildren(), *ftsread();
int ftsclose(), ftsset();

/*
 * Copyright (c) 1980 Regents of the University of California.
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
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char sccsid[] = "@(#)names.c	5.11 (Berkeley) %G%";
#endif /* not lint */

/*
 * Mail -- a mail program
 *
 * Handle name lists.
 */

#include "rcv.h"
#include <sys/wait.h>

/*
 * Allocate a single element of a name list,
 * initialize its name field to the passed
 * name and return it.
 */
struct name *
nalloc(str, ntype)
	char str[];
{
	register struct name *np;

	np = (struct name *) salloc(sizeof *np);
	np->n_flink = NIL;
	np->n_blink = NIL;
	np->n_type = ntype;
	np->n_name = savestr(str);
	return(np);
}

/*
 * Find the tail of a list and return it.
 */
struct name *
tailof(name)
	struct name *name;
{
	register struct name *np;

	np = name;
	if (np == NIL)
		return(NIL);
	while (np->n_flink != NIL)
		np = np->n_flink;
	return(np);
}

/*
 * Extract a list of names from a line,
 * and make a list of names from it.
 * Return the list or NIL if none found.
 */
struct name *
extract(line, ntype)
	char line[];
{
	register char *cp;
	register struct name *top, *np, *t;
	char nbuf[BUFSIZ];

	if (line == NOSTR || *line == '\0')
		return NIL;
	top = NIL;
	np = NIL;
	cp = line;
	while ((cp = yankword(cp, nbuf)) != NOSTR) {
		t = nalloc(nbuf, ntype);
		if (top == NIL)
			top = t;
		else
			np->n_flink = t;
		t->n_blink = np;
		np = t;
	}
	return top;
}

/*
 * Turn a list of names into a string of the same names.
 */
char *
detract(np, ntype)
	register struct name *np;
{
	register int s;
	register char *cp, *top;
	register struct name *p;
	register int comma;

	comma = ntype & GCOMMA;
	if (np == NIL)
		return(NOSTR);
	ntype &= ~GCOMMA;
	s = 0;
	if (debug && comma)
		fprintf(stderr, "detract asked to insert commas\n");
	for (p = np; p != NIL; p = p->n_flink) {
		if (ntype && (p->n_type & GMASK) != ntype)
			continue;
		s += strlen(p->n_name) + 1;
		if (comma)
			s++;
	}
	if (s == 0)
		return(NOSTR);
	s += 2;
	top = salloc(s);
	cp = top;
	for (p = np; p != NIL; p = p->n_flink) {
		if (ntype && (p->n_type & GMASK) != ntype)
			continue;
		cp = copy(p->n_name, cp);
		if (comma && p->n_flink != NIL)
			*cp++ = ',';
		*cp++ = ' ';
	}
	*--cp = 0;
	if (comma && *--cp == ',')
		*cp = 0;
	return(top);
}

/*
 * Grab a single word (liberal word)
 * Throw away things between ()'s, and take anything between <>.
 */
char *
yankword(ap, wbuf)
	char *ap, wbuf[];
{
	register char *cp, *cp2;

	cp = ap;
	for (;;) {
		if (*cp == '\0')
			return NOSTR;
		if (*cp == '(') {
			register int nesting = 0;

			while (*cp != '\0') {
				switch (*cp++) {
				case '(':
					nesting++;
					break;
				case ')':
					--nesting;
					break;
				}
				if (nesting <= 0)
					break;
			}
		} else if (*cp == ' ' || *cp == '\t' || *cp == ',')
			cp++;
		else
			break;
	}
	if (*cp ==  '<')
		for (cp2 = wbuf; *cp && (*cp2++ = *cp++) != '>';)
			;
	else
		for (cp2 = wbuf; *cp && !any(*cp, " \t,("); *cp2++ = *cp++)
			;
	*cp2 = '\0';
	return cp;
}

/*
 * For each recipient in the passed name list with a /
 * in the name, append the message to the end of the named file
 * and remove him from the recipient list.
 *
 * Recipients whose name begins with | are piped through the given
 * program and removed.
 */
struct name *
outof(names, fo, hp)
	struct name *names;
	FILE *fo;
	struct header *hp;
{
	register int c;
	register struct name *np, *top;
#ifdef CRAZYWOW
	register struct name *t, *x;
#endif
	time_t now, time();
	char *date, *fname, *shell, *ctime();
	FILE *fout, *fin;
	int ispipe;
	extern char tempEdit[];
	union wait s;

	top = names;
	np = names;
	(void) time(&now);
	date = ctime(&now);
	while (np != NIL) {
		if (!isfileaddr(np->n_name) && np->n_name[0] != '|') {
			np = np->n_flink;
			continue;
		}
		ispipe = np->n_name[0] == '|';
		if (ispipe)
			fname = np->n_name+1;
		else
			fname = expand(np->n_name);

		/*
		 * See if we have copied the complete message out yet.
		 * If not, do so.
		 */

		if (image < 0) {
			if ((fout = fopen(tempEdit, "a")) == NULL) {
				perror(tempEdit);
				senderr++;
				goto cant;
			}
			image = open(tempEdit, 2);
			(void) unlink(tempEdit);
			if (image < 0) {
				perror(tempEdit);
				senderr++;
				goto cant;
			}
			else {
				rewind(fo);
				fprintf(fout, "From %s %s", myname, date);
				puthead(hp, fout, GTO|GSUBJECT|GCC|GNL);
				while ((c = getc(fo)) != EOF)
					(void) putc(c, fout);
				rewind(fo);
				(void) putc('\n', fout);
				(void) fflush(fout);
				if (ferror(fout))
					perror(tempEdit);
				(void) fclose(fout);
			}
		}

		/*
		 * Now either copy "image" to the desired file
		 * or give it as the standard input to the desired
		 * program as appropriate.
		 */

		if (ispipe) {
			(void) wait(&s);
			if ((shell = value("SHELL")) == NOSTR)
				shell = SHELL;
			if (start_command(shell,
					sigmask(SIGHUP)|sigmask(SIGINT)|
							sigmask(SIGQUIT),
					image, -1, "-c", fname, NOSTR) < 0) {
				senderr++;
				goto cant;
			}
		} else {
			if ((fout = fopen(fname, "a")) == NULL) {
				perror(fname);
				senderr++;
				goto cant;
			}
			fin = Fdopen(image, "r");
			if (fin == NULL) {
				fprintf(stderr, "Can't reopen image\n");
				(void) fclose(fout);
				senderr++;
				goto cant;
			}
			rewind(fin);
			while ((c = getc(fin)) != EOF)
				(void) putc(c, fout);
			if (ferror(fout))
				senderr++, perror(fname);
			(void) fclose(fout);
			(void) fclose(fin);
		}

cant:

		/*
		 * In days of old we removed the entry from the
		 * the list; now for sake of header expansion
		 * we leave it in and mark it as deleted.
		 */

#ifdef CRAZYWOW
		if (np == top) {
			top = np->n_flink;
			if (top != NIL)
				top->n_blink = NIL;
			np = top;
			continue;
		}
		x = np->n_blink;
		t = np->n_flink;
		x->n_flink = t;
		if (t != NIL)
			t->n_blink = x;
		np = t;
#endif

		np->n_type |= GDEL;
		np = np->n_flink;
	}
	if (image >= 0) {
		(void) close(image);
		image = -1;
	}
	return(top);
}

/*
 * Determine if the passed address is a local "send to file" address.
 * If any of the network metacharacters precedes any slashes, it can't
 * be a filename.  We cheat with .'s to allow path names like ./...
 */
isfileaddr(name)
	char *name;
{
	register char *cp;

	if (*name == '+')
		return 1;
	for (cp = name; *cp; cp++) {
		if (*cp == '!' || *cp == '%' || *cp == '@')
			return 0;
		if (*cp == '/')
			return 1;
	}
	return 0;
}

/*
 * Map all of the aliased users in the invoker's mailrc
 * file and insert them into the list.
 * Changed after all these months of service to recursively
 * expand names (2/14/80).
 */

struct name *
usermap(names)
	struct name *names;
{
	register struct name *new, *np, *cp;
	struct grouphead *gh;
	register int metoo;

	new = NIL;
	np = names;
	metoo = (value("metoo") != NOSTR);
	while (np != NIL) {
		if (np->n_name[0] == '\\') {
			cp = np->n_flink;
			new = put(new, np);
			np = cp;
			continue;
		}
		gh = findgroup(np->n_name);
		cp = np->n_flink;
		if (gh != NOGRP)
			new = gexpand(new, gh, metoo, np->n_type);
		else
			new = put(new, np);
		np = cp;
	}
	return(new);
}

/*
 * Recursively expand a group name.  We limit the expansion to some
 * fixed level to keep things from going haywire.
 * Direct recursion is not expanded for convenience.
 */

struct name *
gexpand(nlist, gh, metoo, ntype)
	struct name *nlist;
	struct grouphead *gh;
{
	struct group *gp;
	struct grouphead *ngh;
	struct name *np;
	static int depth;
	char *cp;

	if (depth > MAXEXP) {
		printf("Expanding alias to depth larger than %d\n", MAXEXP);
		return(nlist);
	}
	depth++;
	for (gp = gh->g_list; gp != NOGE; gp = gp->ge_link) {
		cp = gp->ge_name;
		if (*cp == '\\')
			goto quote;
		if (strcmp(cp, gh->g_name) == 0)
			goto quote;
		if ((ngh = findgroup(cp)) != NOGRP) {
			nlist = gexpand(nlist, ngh, metoo, ntype);
			continue;
		}
quote:
		np = nalloc(cp, ntype);
		/*
		 * At this point should allow to expand
		 * to self if only person in group
		 */
		if (gp == gh->g_list && gp->ge_link == NOGE)
			goto skip;
		if (!metoo && strcmp(cp, myname) == 0)
			np->n_type |= GDEL;
skip:
		nlist = put(nlist, np);
	}
	depth--;
	return(nlist);
}

/*
 * Concatenate the two passed name lists, return the result.
 */
struct name *
cat(n1, n2)
	struct name *n1, *n2;
{
	register struct name *tail;

	if (n1 == NIL)
		return(n2);
	if (n2 == NIL)
		return(n1);
	tail = tailof(n1);
	tail->n_flink = n2;
	n2->n_blink = tail;
	return(n1);
}

/*
 * Unpack the name list onto a vector of strings.
 * Return an error if the name list won't fit.
 */
char **
unpack(np)
	struct name *np;
{
	register char **ap, **top;
	register struct name *n;
	int t, extra, metoo, verbose;

	n = np;
	if ((t = count(n)) == 0)
		panic("No names to unpack");
	/*
	 * Compute the number of extra arguments we will need.
	 * We need at least two extra -- one for "mail" and one for
	 * the terminating 0 pointer.  Additional spots may be needed
	 * to pass along -f to the host mailer.
	 */
	extra = 2;
	extra++;
	metoo = value("metoo") != NOSTR;
	if (metoo)
		extra++;
	verbose = value("verbose") != NOSTR;
	if (verbose)
		extra++;
	top = (char **) salloc((t + extra) * sizeof *top);
	ap = top;
	*ap++ = "send-mail";
	*ap++ = "-i";
	if (metoo)
		*ap++ = "-m";
	if (verbose)
		*ap++ = "-v";
	while (n != NIL) {
		if (n->n_type & GDEL) {
			n = n->n_flink;
			continue;
		}
		*ap++ = n->n_name;
		n = n->n_flink;
	}
	*ap = NOSTR;
	return(top);
}

/*
 * Remove all of the duplicates from the passed name list by
 * insertion sorting them, then checking for dups.
 * Return the head of the new list.
 */
struct name *
elide(names)
	struct name *names;
{
	register struct name *np, *t, *new;
	struct name *x;

	if (names == NIL)
		return(NIL);
	new = names;
	np = names;
	np = np->n_flink;
	if (np != NIL)
		np->n_blink = NIL;
	new->n_flink = NIL;
	while (np != NIL) {
		t = new;
		while (nstrcmp(t->n_name, np->n_name) < 0) {
			if (t->n_flink == NIL)
				break;
			t = t->n_flink;
		}

		/*
		 * If we ran out of t's, put the new entry after
		 * the current value of t.
		 */

		if (nstrcmp(t->n_name, np->n_name) < 0) {
			t->n_flink = np;
			np->n_blink = t;
			t = np;
			np = np->n_flink;
			t->n_flink = NIL;
			continue;
		}

		/*
		 * Otherwise, put the new entry in front of the
		 * current t.  If at the front of the list,
		 * the new guy becomes the new head of the list.
		 */

		if (t == new) {
			t = np;
			np = np->n_flink;
			t->n_flink = new;
			new->n_blink = t;
			t->n_blink = NIL;
			new = t;
			continue;
		}

		/*
		 * The normal case -- we are inserting into the
		 * middle of the list.
		 */

		x = np;
		np = np->n_flink;
		x->n_flink = t;
		x->n_blink = t->n_blink;
		t->n_blink->n_flink = x;
		t->n_blink = x;
	}

	/*
	 * Now the list headed up by new is sorted.
	 * Go through it and remove duplicates.
	 */

	np = new;
	while (np != NIL) {
		t = np;
		while (t->n_flink!=NIL &&
		    icequal(np->n_name,t->n_flink->n_name))
			t = t->n_flink;
		if (t == np || t == NIL) {
			np = np->n_flink;
			continue;
		}
		
		/*
		 * Now t points to the last entry with the same name
		 * as np.  Make np point beyond t.
		 */

		np->n_flink = t->n_flink;
		if (t->n_flink != NIL)
			t->n_flink->n_blink = np;
		np = np->n_flink;
	}
	return(new);
}

/*
 * Version of strcmp which ignores case differences.
 */
nstrcmp(s1, s2)
	register char *s1, *s2;
{
	register int c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;
	} while (c1 && c1 == c2);
	return(c1 - c2);
}

/*
 * Put another node onto a list of names and return
 * the list.
 */
struct name *
put(list, node)
	struct name *list, *node;
{
	node->n_flink = list;
	node->n_blink = NIL;
	if (list != NIL)
		list->n_blink = node;
	return(node);
}

/*
 * Determine the number of elements in
 * a name list and return it.
 */
count(np)
	register struct name *np;
{
	register int c;

	for (c = 0; np != NIL; c++, np = np->n_flink)
		;
	return c;
}

/*
 * Delete the given name from a namelist, using the passed
 * function to compare the names.
 */
struct name *
delname(np, name, cmpfun)
	register struct name *np;
	char name[];
	int (* cmpfun)();
{
	register struct name *p;

	for (p = np; p != NIL; p = p->n_flink)
		if ((* cmpfun)(p->n_name, name)) {
			if (p->n_blink == NIL) {
				if (p->n_flink != NIL)
					p->n_flink->n_blink = NIL;
				np = p->n_flink;
				continue;
			}
			if (p->n_flink == NIL) {
				if (p->n_blink != NIL)
					p->n_blink->n_flink = NIL;
				continue;
			}
			p->n_blink->n_flink = p->n_flink;
			p->n_flink->n_blink = p->n_blink;
		}
	return(np);
}

/*
 * Pretty print a name list
 * Uncomment it if you need it.
 */

/*
prettyprint(name)
	struct name *name;
{
	register struct name *np;

	np = name;
	while (np != NIL) {
		fprintf(stderr, "%s(%d) ", np->n_name, np->n_type);
		np = np->n_flink;
	}
	fprintf(stderr, "\n");
}
*/

#ifndef lint
static char sccsid[] = "@(#)ttgeneric.c	3.28 %G%";
#endif

/*
 * Copyright (c) 1983 Regents of the University of California,
 * All rights reserved.  Redistribution permitted subject to
 * the terms of the Berkeley Software License Agreement.
 */

#include "ww.h"
#include "tt.h"

char PC, *BC, *UP;
short ospeed;

	/* normal frame */
short gen_frame[16] = {
	' ', '|', '-', '+',
	'|', '|', '+', '+',
	'-', '+', '-', '+',
	'+', '+', '+', '+'
};

	/* ANSI graphics frame */
#define G (WWM_GRP << WWC_MSHIFT)
short ansi_frame[16] = {
	' ',	'x'|G,	'Q'|G,	'm'|G,
	'x'|G,	'x'|G,	'l'|G,	't'|G,
	'q'|G,	'j'|G,	'q'|G,	'v'|G,
	'k'|G,	'u'|G,	'w'|G,	'n'|G
};
#define ANSI_AS "\033(0"	/* ) */

struct tt_str *gen_CM;
struct tt_str *gen_IM;
struct tt_str *gen_IC;
struct tt_str *gen_IP;
struct tt_str *gen_EI;
struct tt_str *gen_DC;
struct tt_str *gen_AL;
struct tt_str *gen_DL;
struct tt_str *gen_CE;
struct tt_str *gen_CD;
struct tt_str *gen_CL;
struct tt_str *gen_VS;
struct tt_str *gen_VE;
struct tt_str *gen_TI;
struct tt_str *gen_TE;
struct tt_str *gen_SO;
struct tt_str *gen_SE;
struct tt_str *gen_US;
struct tt_str *gen_UE;
struct tt_str *gen_UP;
struct tt_str *gen_PC;
struct tt_str *gen_BC;
struct tt_str *gen_ND;
struct tt_str *gen_HO;
struct tt_str *gen_NL;
struct tt_str *gen_AS;
struct tt_str *gen_AE;
char gen_MI;
char gen_MS;
char gen_AM;
char gen_OS;
char gen_BS;
char gen_DB;
int gen_CO;
int gen_LI;
int gen_UG;
int gen_SG;

gen_setinsert(new)
char new;
{
	if (new) {
		if (gen_IM)
			ttxputs(gen_IM);
	} else
		if (gen_EI)
			ttxputs(gen_EI);
	tt.tt_insert = new;
}

gen_setmodes(new)
register new;
{
	register diff;

	diff = new ^ tt.tt_modes;
	if (diff & WWM_REV) {
		if (new & WWM_REV) {
			if (gen_SO)
				ttxputs(gen_SO);
		} else
			if (gen_SE)
				ttxputs(gen_SE);
	}
	if (diff & WWM_UL) {
		if (new & WWM_UL) {
			if (gen_US)
				ttxputs(gen_US);
		} else
			if (gen_UE)
				ttxputs(gen_UE);
	}
	if (diff & WWM_GRP) {
		if (new & WWM_GRP) {
			if (gen_AS)
				ttxputs(gen_AS);
		} else
			if (gen_AE)
				ttxputs(gen_AE);
	}
	tt.tt_modes = new;
}

gen_insline()
{
	if (tt.tt_modes)			/* for concept 100 */
		gen_setmodes(0);
	if (gen_AL)
		tttputs(gen_AL, gen_LI - tt.tt_row);
}

gen_delline()
{
	if (tt.tt_modes)			/* for concept 100 */
		gen_setmodes(0);
	if (gen_DL)
		tttputs(gen_DL, gen_LI - tt.tt_row);
}

gen_putc(c)
register char c;
{
	if (tt.tt_ninsert != tt.tt_insert)
		gen_setinsert(tt.tt_ninsert);
	if (tt.tt_nmodes != tt.tt_modes)
		gen_setmodes(tt.tt_nmodes);
	if (tt.tt_insert) {
		if (gen_IC)
			tttputs(gen_IC, gen_CO - tt.tt_col);
		ttputc(c);
		if (gen_IP)
			tttputs(gen_IP, gen_CO - tt.tt_col);
	} else
		ttputc(c);
	if (++tt.tt_col == gen_CO)
		if (gen_AM)
			tt.tt_col = 0, tt.tt_row++;
		else
			tt.tt_col--;
}

gen_write(p, n)
	register char *p;
	register n;
{
	if (tt.tt_ninsert != tt.tt_insert)
		gen_setinsert(tt.tt_ninsert);
	if (tt.tt_nmodes != tt.tt_modes)
		gen_setmodes(tt.tt_nmodes);
	if (tt.tt_insert && (gen_IC || gen_IP)) {
		while (--n >= 0) {
			if (gen_IC)
				tttputs(gen_IC, gen_CO - tt.tt_col);
			ttputc(*p++);
			if (gen_IP)
				tttputs(gen_IP, gen_CO - tt.tt_col);
			tt.tt_col++;
		}
	} else {
		tt.tt_col += n;
		ttwrite(p, n);
	}
	if (tt.tt_col == gen_CO)
		if (gen_AM)
			tt.tt_col = 0, tt.tt_row++;
		else
			tt.tt_col--;
}

gen_move(row, col)
register char row, col;
{
	if (tt.tt_row == row && tt.tt_col == col)
		return;
	if (!gen_MI && tt.tt_insert)
		gen_setinsert(0);
	if (!gen_MS && tt.tt_modes)
		gen_setmodes(0);
	if (tt.tt_row == row) {
		if (tt.tt_col == col)
			return;
		if (tt.tt_col == col - 1) {
			if (gen_ND) {
				ttxputs(gen_ND);
				goto out;
			}
		} else if (tt.tt_col == col + 1) {
			if (gen_BC) {
				ttxputs(gen_BC);
				goto out;
			}
		}
	}
	if (tt.tt_col == col) {
		if (tt.tt_row == row + 1) {
			if (gen_UP) {
				ttxputs(gen_UP);
				goto out;
			}
		} else if (tt.tt_row == row + 1) {
			ttxputs(gen_NL);
			goto out;
		}
	}
	if (gen_HO && col == 0 && row == 0) {
		ttxputs(gen_HO);
		goto out;
	}
	tttgoto(gen_CM, col, row);
out:
	tt.tt_col = col;
	tt.tt_row = row;
}

gen_init()
{
	if (gen_VS)
		ttxputs(gen_VS);
	if (gen_TI)
		ttxputs(gen_TI);
	ttxputs(gen_CL);
	tt.tt_col = tt.tt_row = 0;
	tt.tt_ninsert = tt.tt_insert = 0;
	tt.tt_nmodes = tt.tt_modes = 0;
}

gen_end()
{
	if (gen_TE)
		ttxputs(gen_TE);
	if (gen_VE)
		ttxputs(gen_VE);
}

gen_clreol()
{
	if (tt.tt_modes)			/* for concept 100 */
		gen_setmodes(0);
	tttputs(gen_CE, gen_CO - tt.tt_col);
}

gen_clreos()
{
	if (tt.tt_modes)			/* for concept 100 */
		gen_setmodes(0);
	tttputs(gen_CD, gen_LI - tt.tt_row);
}

gen_clear()
{
	if (tt.tt_modes)			/* for concept 100 */
		gen_setmodes(0);
	ttxputs(gen_CL);
}

gen_delchar()
{
	tttputs(gen_DC, gen_CO - tt.tt_col);
}

tt_generic()
{
	gen_PC = tttgetstr("pc");
	PC = gen_PC ? *gen_PC->ts_str : 0;
	ospeed = wwoldtty.ww_sgttyb.sg_ospeed;

	gen_CM = ttxgetstr("cm");		/* may not work */
	gen_IM = ttxgetstr("im");
	gen_IC = tttgetstr("ic");
	gen_IP = tttgetstr("ip");
	gen_EI = ttxgetstr("ei");
	gen_DC = tttgetstr("dc");
	gen_AL = tttgetstr("al");
	gen_DL = tttgetstr("dl");
	gen_CE = tttgetstr("ce");
	gen_CD = tttgetstr("cd");
	gen_CL = ttxgetstr("cl");
	gen_VS = ttxgetstr("vs");
	gen_VE = ttxgetstr("ve");
	gen_TI = ttxgetstr("ti");
	gen_TE = ttxgetstr("te");
	gen_SO = ttxgetstr("so");
	gen_SE = ttxgetstr("se");
	gen_US = ttxgetstr("us");
	gen_UE = ttxgetstr("ue");
	gen_UP = ttxgetstr("up");
	gen_BC = ttxgetstr("bc");
	gen_ND = ttxgetstr("nd");
	gen_HO = ttxgetstr("ho");
	gen_NL = ttxgetstr("nl");
	gen_AS = ttxgetstr("as");
	gen_AE = ttxgetstr("ae");
	gen_MI = tgetflag("mi");
	gen_MS = tgetflag("ms");
	gen_AM = tgetflag("am");
	gen_OS = tgetflag("os");
	gen_BS = tgetflag("bs");
	gen_DB = tgetflag("db");
	gen_CO = tgetnum("co");
	gen_LI = tgetnum("li");
	gen_UG = tgetnum("ug");
	gen_SG = tgetnum("sg");

	if (gen_NL == 0) {
		static struct tt_str nl = { "\n", 1 };
		gen_NL = &nl;
	}
	if (gen_BC == 0 && gen_BS) {
		static struct tt_str bc = { "\b", 1 };
		gen_BC = &bc;
	}
	BC = gen_BC ? gen_BC->ts_str : 0;
	UP = gen_UP ? gen_UP->ts_str : 0;
	if (gen_CL == 0 || gen_OS || gen_CM == 0)
		return -1;

	if (gen_DC)
		tt.tt_delchar = gen_delchar;
	if (gen_AL)
		tt.tt_insline = gen_insline;
	if (gen_DL)
		tt.tt_delline = gen_delline;
	if (gen_CE)
		tt.tt_clreol = gen_clreol;
	if (gen_CD)
		tt.tt_clreos = gen_clreos;
	if (gen_SG > 0)
		gen_SO = 0;
	if (gen_UG > 0)
		gen_US = 0;
	if (gen_SO)
		tt.tt_availmodes |= WWM_REV;
	if (gen_US)
		tt.tt_availmodes |= WWM_UL;
	if (gen_AS)
		tt.tt_availmodes |= WWM_GRP;
	tt.tt_hasinsert = gen_IM != 0;
	tt.tt_wrap = gen_AM;
	tt.tt_retain = gen_DB;
	tt.tt_ncol = gen_CO;
	tt.tt_nrow = gen_LI;
	tt.tt_init = gen_init;
	tt.tt_end = gen_end;
	tt.tt_write = gen_write;
	tt.tt_putc = gen_putc;
	tt.tt_move = gen_move;
	tt.tt_clear = gen_clear;
	tt.tt_setinsert = gen_setinsert;
	tt.tt_setmodes = gen_setmodes;
	tt.tt_frame = gen_AS && !strncmp(gen_AS->ts_str, ANSI_AS, gen_AS->ts_n)
		? ansi_frame : gen_frame;
	return 0;
}

/*
 * Copyright (c) 2002 Matteo Frigo
 * Copyright (c) 2002 Steven G. Johnson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/* $Id: planner-score.c,v 1.18 2002-09-01 23:22:53 athena Exp $ */
#include "ifftw.h"

typedef struct {
     visit_closure super;
     int sc;
} adjclosure;

static void visit(visit_closure *k_, plan *p)
{
     adjclosure *k = (adjclosure *)k_;
     if (p->score < k->sc) k->sc = p->score; 
}

static void adjust_score(plan *pln)
{
     adjclosure k;
     k.super.visit = visit;
     k.sc = pln->score;
     X(traverse_plan)(pln, 0, &k.super);
     pln->score = k.sc;
}

static void mkplan(planner *ego, problem *p, plan **bestp, slvdesc **descp)
{
     plan *best = 0;
     int best_score;
     int best_not_yet_timed = 1;
     slvdesc *sp;

     *bestp = 0;

     /* if a solver is suggested (by wisdom) use it */
     if ((sp = *descp)) {
	  solver *s = sp->slv;
	  best = ego->adt->slv_mkplan(ego, p, s);
	  if (best) {
	       best->score = s->adt->score(s, p, ego);
	       adjust_score(best);
	       *bestp = best;
	       return;
	  }
	  /* BEST may be 0 in case of md5 collision */
     }

     best_score = BAD;
     FORALL_SOLVERS(ego, s, sp, {
	  int sc = s->adt->score(s, p, ego);
	  if (sc > best_score) 
	       best_score = sc;
     });

     for (; best_score > BAD; --best_score) {
          FORALL_SOLVERS(ego, s, sp, {
	       if (s->adt->score(s, p, ego) == best_score) {
		    plan *pln = ego->adt->slv_mkplan(ego, p, s);

		    if (pln) {
			 X(plan_use)(pln);

			 if (best) {
			      if (best_not_yet_timed) {
				   X(evaluate_plan)(ego, best, p);
				   best_not_yet_timed = 0;
			      }
			      X(evaluate_plan)(ego, pln, p);
			      if (pln->pcost < best->pcost) {
				   X(plan_destroy)(best);
				   best = pln;
				   *descp = sp;
			      } else {
				   X(plan_destroy)(pln);
			      }
			 } else {
			      best = pln;
			      *descp = sp;
			 }
		    }
	       }
	  });

	  if (best) {
	       best->score = best_score;
	       adjust_score(best);
	       *bestp = best;
	       if (best->score >= best_score)
		    break;
	  }
     };
}

/* constructor */
planner *X(mkplanner_score)(uint flags)
{
     return X(mkplanner)(sizeof(planner), mkplan, 0, flags);
}

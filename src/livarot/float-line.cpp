/**
 *  \file livarot/float-line.cpp
 *
 *  \author Fred
 *
 *  public domain
 *
 */

#include <cstdio>
#include "livarot/float-line.h"
#include "livarot/int-line.h"

FloatLigne::FloatLigne()
{
    firstAc = lastAc = -1;
    s_first = s_last = -1;
}


FloatLigne::~FloatLigne()
{
    
}


void FloatLigne::Reset()
{
    bords.clear();
    runs.clear();
    firstAc = lastAc = -1;
    s_first = s_last = -1;
}


int FloatLigne::AddBord(float spos, float sval, float epos, float eval, int guess)
{
//  if ( showCopy ) printf("b= %f %f -> %f %f \n",spos,sval,epos,eval);
    if ( spos >= epos ) {
        return -1;
    }

    float pente = (eval - sval) / (epos - spos);
    
#ifdef faster_flatten
    if ( fabsf(epos-spos) < 0.001 || fabsf(pente) > 1000 ) {
        return;
        epos = spos;
        pente = 0;
    }
#endif
  
    if ( guess >= int(bords.size()) ) {
        guess = -1;
    }
    
    // add the left boundary
    float_ligne_bord b;
    int n = bords.size();
    b.pos = spos;
    b.val = sval;
    b.start = true;
    b.other = n + 1;
    b.pente = pente;
    b.prev = b.next = -1;
    b.s_prev = b.s_next = -1;
    bords.push_back(b);

    // insert it in the doubly-linked list
    InsertBord(n, spos, guess);
    
    // add the right boundary
    n = bords.size();
    b.pos = epos;
    b.val = eval;
    b.start = false;
    b.other = n-1;
    b.pente = pente;
    b.prev = b.next = -1;
    b.s_prev = b.s_next = -1;
    bords.push_back(b);
    
    // insert it in the doubly-linked list, knowing that boundary at index n-1 is not too far before me
    InsertBord(n, epos, n - 1);
  	
    return n;
}


int FloatLigne::AddBord(float spos, float sval, float epos, float eval, float pente, int guess)
{
//  if ( showCopy ) printf("b= %f %f -> %f %f \n",spos,sval,epos,eval);
    if ( spos >= epos ) {
        return -1;
    }

#ifdef faster_flatten
    if ( fabsf(epos-spos) < 0.001 || fabsf(pente) > 1000 ) {
        return;
        epos = spos;
        pente = 0;
    }
#endif
    
    if ( guess >= int(bords.size()) ) {
        guess=-1;
    }

    float_ligne_bord b;
    int n = bords.size();
    b.pos = spos;
    b.val = sval;
    b.start = true;
    b.other = n + 1;
    b.pente = pente;
    b.prev = b.next = -1;
    b.s_prev = b.s_next = -1;
    bords.push_back(b);

    n = bords.size();
    b.pos = epos;
    b.val = eval;
    b.start = false;
    b.other = n - 1;
    b.pente = pente;
    b.prev = b.next = -1;
    b.s_prev = b.s_next = -1;
    bords.push_back(b);

    InsertBord(n - 1, spos, guess);
    InsertBord(n, epos, n - 1);
/*	if ( bords[n-1].s_next < 0 ) {
		bords[n].s_next=-1;
		s_last=n;

		bords[n].s_prev=n-1;
		bords[n-1].s_next=n;
	} else if ( bords[bords[n-1].s_next].pos >= epos ) {
		bords[n].s_next=bords[n-1].s_next;
		bords[bords[n].s_next].s_prev=n;
		
		bords[n].s_prev=n-1;
		bords[n-1].s_next=n;
	} else {
		int c=bords[bords[n-1].s_next].s_next;
		while ( c >= 0 && bords[c].pos < epos ) c=bords[c].s_next;
		if ( c < 0 ) {
			bords[n].s_prev=s_last;
			bords[s_last].s_next=n;
			s_last=n;
		} else {
			bords[n].s_prev=bords[c].s_prev;
			bords[bords[n].s_prev].s_next=n;

			bords[n].s_next=c;
			bords[c].s_prev=n;
		}

	}*/
    return n;
}


int FloatLigne::AddBordR(float spos, float sval, float epos, float eval, float pente, int guess)
{
//  if ( showCopy ) printf("br= %f %f -> %f %f \n",spos,sval,epos,eval);
//	return AddBord(spos,sval,epos,eval,pente,guess);
    if ( spos >= epos ){
        return -1;
    }
    
#ifdef faster_flatten
    if ( fabsf(epos-spos) < 0.001 || fabsf(pente) > 1000 ) {
        return;
        epos = spos;
        pente = 0;
    }
#endif

    if ( guess >= int(bords.size()) ) {
        guess=-1;
    }

    float_ligne_bord b;
    int n = bords.size();
    b.pos = spos;
    b.val = sval;
    b.start = true;
    b.other = n + 1;
    b.pente = pente;
    b.prev = b.next = -1;
    b.s_prev = b.s_next = -1;
    bords.push_back(b);
    
    n = bords.size();
    b.pos = epos;
    b.val = eval;
    b.start = false;
    b.other = n - 1;
    b.pente = pente;
    b.prev = b.next = -1;
    b.s_prev = b.s_next = -1;
    bords.push_back(b);
    
    InsertBord(n, epos, guess);
    InsertBord(n - 1, spos, n);
    
/*	if ( bords[n].s_prev < 0 ) {
		bords[n-1].s_prev=-1;
		s_first=n-1;

		bords[n-1].s_next=n;
		bords[n].s_prev=n-1;
	} else if ( bords[bords[n].s_prev].pos <= spos ) {
		bords[n-1].s_prev=bords[n].s_prev;
		bords[bords[n-1].s_prev].s_next=n-1;

		bords[n-1].s_next=n;
		bords[n].s_prev=n-1;
	} else {
		int c=bords[bords[n].s_prev].s_prev;
		while ( c >= 0 && bords[c].pos > spos ) c=bords[c].s_prev;
		if ( c < 0 ) {
			bords[n-1].s_next=s_first;
			bords[s_first].s_prev=n-1;
			s_first=n-1;
		} else {
			bords[n-1].s_next=bords[c].s_next;
			bords[bords[n-1].s_next].s_prev=n-1;

			bords[n-1].s_prev=c;
			bords[c].s_next=n-1;
		}
		
                }*/
    return n - 1;
}


// variant where insertion is known to be trivial: just append to the list
int FloatLigne::AppendBord(float spos, float sval, float epos, float eval, float pente)
{
//  if ( showCopy ) printf("b= %f %f -> %f %f \n",spos,sval,epos,eval);
//	return AddBord(spos,sval,epos,eval,pente,s_last);
    if ( spos >= epos ) {
        return -1;
    }
    
#ifdef faster_flatten
    if ( fabsf(epos-spos) < 0.001 || fabsf(pente) > 1000 ) {
        return;
        epos=spos;
        pente=0;
    }
#endif
    
    int n = bords.size();
    float_ligne_bord b;
    b.pos = spos;
    b.val = sval;
    b.start = true;
    b.other = n + 1;
    b.pente = pente;
    b.prev = b.next = -1;
    b.s_prev = s_last;
    b.s_next = n + 1;
    bords.push_back(b);
 
    if ( s_last >=  0 ) {
        bords[s_last].s_next = n;
    }
    
    if ( s_first < 0 ) {
        s_first = n;
    }

    n = bords.size();
    b.pos = epos;
    b.val = eval;
    b.start = false;
    b.other = n - 1;
    b.pente = pente;
    b.prev = b.next = -1;
    b.s_prev = n - 1;
    b.s_next = -1;
    bords.push_back(b);

    s_last = n;

    return n;
}



// insertion in a boubly-linked list. nothing interesting here
void FloatLigne::InsertBord(int no, float p, int guess)
{
    if ( no < 0 || no >= int(bords.size()) ) {
        return;
    }
    
    if ( s_first < 0 ) {
        s_first = s_last = no;
        bords[no].s_prev = -1;
        bords[no].s_next = -1;
        return;
    }
    
    if ( guess < 0 || guess >= int(bords.size()) ) {
        int c = s_first;
        while ( c >= 0 && c < int(bords.size()) && CmpBord(bords[c], bords[no]) < 0 ) {
            c = bords[c].s_next;
        }
        
        if ( c < 0 || c >= int(bords.size()) ) {
            bords[no].s_prev = s_last;
            bords[s_last].s_next = no;
            s_last = no;
        } else {
            bords[no].s_prev = bords[c].s_prev;
            if ( bords[no].s_prev >= 0 ) {
                bords[bords[no].s_prev].s_next = no;
            } else {
                s_first = no;
            }
            bords[no].s_next = c;
            bords[c].s_prev = no;
        }
    } else {
        int c = guess;
        int stTst = CmpBord(bords[c], bords[no]);

        if ( stTst == 0 ) {

            bords[no].s_prev = bords[c].s_prev;
            if ( bords[no].s_prev >= 0 ) {
                bords[bords[no].s_prev].s_next = no;
            } else {
                s_first = no;
            }
            bords[no].s_next = c;
            bords[c].s_prev = no;
            
        } else if ( stTst > 0 ) {
            
            while ( c >= 0 && c < int(bords.size()) && CmpBord(bords[c], bords[no]) > 0 ) {
                c = bords[c].s_prev;
            }
            
            if ( c < 0 || c >= int(bords.size()) ) {
                bords[no].s_next = s_first;
                bords[s_first].s_prev =no; // s_first != -1
                s_first = no; 
            } else {
                bords[no].s_next = bords[c].s_next;
                if ( bords[no].s_next >= 0 ) {
                    bords[bords[no].s_next].s_prev = no;
                } else {
                    s_last = no;
                }
                bords[no].s_prev = c;
                bords[c].s_next = no;
            }
            
        } else {

            while ( c >= 0 && c < int(bords.size()) && CmpBord(bords[c],bords[no]) < 0 ) {
                c = bords[c].s_next;
            }
            
            if ( c < 0 || c >= int(bords.size()) ) {
                bords[no].s_prev = s_last;
                bords[s_last].s_next = no;
                s_last = no;
            } else {
                bords[no].s_prev = bords[c].s_prev;
                if ( bords[no].s_prev >= 0 ) {
                    bords[bords[no].s_prev].s_next = no;
                } else {
                    s_first = no;
                }
                bords[no].s_next = c;
                bords[c].s_prev = no;
            }
        }
    }
}


float FloatLigne::RemainingValAt(float at, int pending)
{
    float sum = 0;
/*	int     no=firstAc;
	while ( no >= 0 && no < bords.size() ) {
		int   nn=bords[no].other;
		sum+=bords[nn].val+(at-bords[nn].pos)*bords[nn].pente;
//				sum+=((at-bords[nn].pos)*bords[no].val+(bords[no].pos-at)*bords[nn].val)/(bords[no].pos-bords[nn].pos);
//		sum+=ValAt(at,bords[nn].pos,bords[no].pos,bords[nn].val,bords[no].val);
		no=bords[no].next;
	}*/
  // for each portion being scanned, compute coverage at position "at" and sum.
  // we could simply compute the sum of portion coverages as a "f(x)=ux+y" and evaluate it at "x=at",
  // but there are numerical problems with this approach, and it produces ugly lines of incorrectly 
  // computed alpha values, so i reverted to this "safe but slow" version
    
    for (int i=0; i < pending; i++) {
        int const nn = bords[i].pend_ind;
        sum += bords[nn].val + (at - bords[nn].pos) * bords[nn].pente;
    }
    
    return sum;
}


// sorting
void FloatLigne::SwapBords(int a, int b)
{
    int oa = bords[a].other;
    int ob = bords[b].other;
  
    float_ligne_bord swap = bords[a];
    bords[a] = bords[b];
    bords[b] = swap;
  
    if ( oa == b ) {
        bords[b].other = a;
    } else {
        bords[oa].other = b;
    }
    
    if ( ob == a ) {
        bords[a].other = b;
    } else {
        bords[ob].other = a;
    }
}


void FloatLigne::SwapBords(int a, int b, int c)
{
    if (a == b || b == c || a == c) {
        return;
    }
    
    SwapBords(a, b);
    SwapBords(b, c);
}

void FloatLigne::SortBords(int s, int e)
{
    if (s >= e) {
        return;
    }
    
    if (e == s + 1)  {
        if ( CmpBord(bords[s],bords[e]) > 0 ) {
            SwapBords (s, e);
        }
        return;
    }
  
    int ppos = (s + e) / 2;
    int plast = ppos;
    float_ligne_bord pval = bords[ppos];
  
    int le = s;
    int ri = e;
    while (le < ppos || ri > plast) {
        if (le < ppos) {
            do {
                int const test = CmpBord(bords[le], pval);
                if (test == 0) {
                    // on colle les valeurs egales au pivot ensemble
                    if (le < ppos - 1) {
                        SwapBords (le, ppos - 1, ppos);
                        ppos--;
                        continue;	// sans changer le
                    } else if (le == ppos - 1) {
                        ppos--;
                        break;
                    } else {
                        // oupsie
                        break;
                    }
                }
                if (test > 0) {
                    break;
                }
                le++;
	    } while (le < ppos);
        }
        
        if (ri > plast) {
            do {
                int const test = CmpBord(bords[ri],pval);
                if (test == 0) {
                    // on colle les valeurs egales au pivot ensemble
                    if (ri > plast + 1) {
                        SwapBords (ri, plast + 1, plast);
                        plast++;
                        continue;	// sans changer ri
                    } else if (ri == plast + 1) {
                        plast++;
                        break;
                    } else {
                        // oupsie
                        break;
                    }
                }
                if (test < 0) {
                    break;
                }
                ri--;
	    } while (ri > plast);
        }
        
        if (le < ppos) {
            if (ri > plast)  {
                SwapBords(le, ri);
                le++;
                ri--;
	    } else {
                if (le < ppos - 1) {
                    SwapBords (ppos - 1, plast, le);
                    ppos--;
                    plast--;
                } else if (le == ppos - 1) {
                    SwapBords (plast, le);
                    ppos--;
                    plast--;
                }
	    }
        } else {
            if (ri > plast + 1) {
                SwapBords (plast + 1, ppos, ri);
                ppos++;
                plast++;
	    }  else if (ri == plast + 1) {
                SwapBords (ppos, ri);
                ppos++;
                plast++;
	    } else {
                break;
	    }
        }
    }

    SortBords(s, ppos - 1);
    SortBords(plast + 1, e);
}



/**
 *    computation of non-overlapping runs of coverage.
 */

void FloatLigne::Flatten()
{
    if ( int(bords.size()) <= 1 ) {
        Reset();
        return;
    }
    
    runs.clear();
    firstAc = lastAc = -1;

//	qsort(bords,bords.size(),sizeof(float_ligne_bord),FloatLigne::CmpBord);
//	SortBords(0,bords.size()-1);
  
    float totPente = 0;
    float totStart = 0;
    float totX = bords[0].pos;
    
    bool startExists = false;
    float lastStart = 0;
    float lastVal = 0;
    int pending = 0;
    
//	for (int i=0;i<bords.size();) {
    // read the list from left to right, adding a run for each boundary crossed, minus runs with alpha=0
    for (int i=/*0*/s_first; i>=0 && i < int(bords.size()) ;) {
        
        float cur = bords[i].pos;  // position of the current boundary (there may be several boundaries at this position)
        float leftV = 0;  // deltas in coverage value at this position
        float rightV = 0;
        float leftP = 0; // deltas in coverage increase per unit length at this position
        float rightP = 0;
        
        // more precisely, leftV is the sum of decreases of coverage value,
        // while rightV is the sum of increases, so that leftV+rightV is the delta.
        // idem for leftP and rightP
    
        // start by scanning all boundaries that end a portion at this position
        while ( i >= 0 && i < int(bords.size()) && bords[i].pos == cur && bords[i].start == false ) {
            leftV += bords[i].val;
            leftP += bords[i].pente;
            
#ifndef faster_flatten
            // we need to remove the boundary that started this coverage portion for the pending list
            if ( bords[i].other >= 0 && bords[i].other < int(bords.size()) ) {
                // so we use the pend_inv "array"
                int const k = bords[bords[i].other].pend_inv;
                if ( k >= 0 && k < pending ) {
                    // and update the pend_ind array and its inverse pend_inv
                    bords[k].pend_ind = bords[pending - 1].pend_ind;
                    bords[bords[k].pend_ind].pend_inv = k;
                }
            }
#endif
            
            // one less portion pending
            pending--;
            // and we move to the next boundary in the doubly linked list
            i=bords[i].s_next;
            //i++;
        }
        
        // then scan all boundaries that start a portion at this position
        while ( i >= 0 && i < int(bords.size()) && bords[i].pos == cur && bords[i].start == true ) {
            rightV += bords[i].val;
            rightP += bords[i].pente;
#ifndef faster_flatten
            bords[pending].pend_ind=i;
            bords[i].pend_inv=pending;
#endif
            pending++;
            i = bords[i].s_next;
            //i++;
        }

        // coverage value at end of the run will be "start coverage"+"delta per unit length"*"length"
        totStart = totStart + totPente * (cur - totX);
    
        if ( startExists ) {
            // add that run
            AddRun(lastStart, cur, lastVal, totStart, totPente);
        }
        // update "delta coverage per unit length"
        totPente += rightP - leftP;
        // not really needed here
        totStart += rightV - leftV;
        // update position
        totX = cur;
        if ( pending > 0 ) {
            startExists = true;
            
#ifndef faster_flatten
            // to avoid accumulation of numerical errors, we compute an accurate coverage for this position "cur"
            totStart = RemainingValAt(cur, pending);
#endif
            lastVal = totStart;
            lastStart = cur;
        } else {
            startExists = false;
            totStart = 0;
            totPente = 0;
        }
    }
}


void FloatLigne::Affiche()
{
    printf("%i : \n", bords.size());
    for (int i = 0; i < int(bords.size()); i++) {
        printf("(%f %f %f %i) ",bords[i].pos,bords[i].val,bords[i].pente,(bords[i].start?1:0)); // localization ok
    }
    
    printf("\n");
    printf("%i : \n", runs.size());
    
    for (int i = 0; i < int(runs.size()); i++) {
        printf("(%f %f -> %f %f / %f)",
               runs[i].st, runs[i].vst, runs[i].en, runs[i].ven, runs[i].pente); // localization ok
    }
    
    printf("\n");
}


int FloatLigne::AddRun(float st, float en, float vst, float ven)
{
    if ( st >= en ) {
        return -1;
    }

/*  if ( nbRun > 0 && st < runs[nbRun-1].en-0.1 ) {
    printf("o");
  }*/
    
    int const n = runs.size();
    float_ligne_run r;
    r.st = st;
    r.en = en;
    r.vst = vst;
    r.ven = ven;
    r.pente = (ven - vst) / (en - st);
    runs.push_back(r);
    
    return n;
}


int FloatLigne::AddRun(float st, float en, float vst, float ven, float pente)
{
    if ( st >= en ) {
        return -1;
    }

/*  if ( nbRun > 0 && st < runs[nbRun-1].en-0.1 ) {
    printf("o");
  }*/
    
    int const n = runs.size();
    float_ligne_run r;
    r.st = st;
    r.en = en;
    r.vst = vst;
    r.ven = ven;
    r.pente = pente;
    runs.push_back(r);
    
    return n;
}

void FloatLigne::Copy(FloatLigne *a)
{
    if ( a->runs.empty() ) {
        Reset();
        return;
    }
    
    bords.clear();
    runs = a->runs;
}

void FloatLigne::Copy(IntLigne *a)
{
    if ( a->nbRun ) {
        Reset();
        return;
    }
    
    bords.clear();
    runs.resize(a->nbRun);
    
    for (int i = 0; i < int(runs.size()); i++) {
        runs[i].st = a->runs[i].st;
        runs[i].en = a->runs[i].en;
        runs[i].vst = a->runs[i].vst;
        runs[i].ven = a->runs[i].ven;
    }
}


// all operation on the FloatLigne instances are done by scanning the runs left to right in the
// source(s) instances, and adding the necessary runs to the solution. thus it reduces to
// computing the operation between float_ligne_run elements
// but details are not pretty (et c'est une litote)

void FloatLigne::Booleen(FloatLigne *a, FloatLigne *b, BooleanOp mod)
{
    Reset();
    
    if ( a->runs.empty() && b->runs.empty() <= 0 ) {
        return;
    }
    
    if ( a->runs.empty() ) {
        if ( mod == bool_op_union || mod == bool_op_symdiff ) {
            Copy(b);
        }
        return;
    }
    
    if ( b->runs.empty() ) {
        if ( mod == bool_op_union || mod == bool_op_diff || mod == bool_op_symdiff ) {
            Copy(a);
        }
        return;
    }

    int curA = 0;
    int curB = 0;
    /* FIXME: this could use std::max */
    float curPos = (a->runs[0].st < b->runs[0].st) ? a->runs[0].st : b->runs[0].st;
    float nextPos = curPos;
    bool inA = false;
    bool inB = false;
    float valA = 0;
    float valB = 0;
    
    if ( curPos == a->runs[0].st ) {
        valA = a->runs[0].vst;
    }
    
    if ( curPos == b->runs[0].st ) {
        valB = b->runs[0].vst;
    }
	
    while ( curA < int(a->runs.size()) && curB < int(b->runs.size()) ) {
        float_ligne_run runA = a->runs[curA];
        float_ligne_run runB = b->runs[curB];
        inA = ( curPos >= runA.st && curPos < runA.en );
        inB = ( curPos >= runB.st && curPos < runB.en );

        bool startA = false;
        bool startB = false;
        bool endA = false;
        bool endB = false;
        
        if ( curPos < runA.st ) {
            
            if ( curPos < runB.st ) {
                startA = runA.st <= runB.st;
                startB = runA.st >= runB.st;
                nextPos = startA ? runA.st : runB.st;
            } else if ( curPos >= runB.st ) {
                startA = runA.st <= runB.en;
                endB = runA.st >= runB.en;
                nextPos = startA ? runA.st : runB.en;
            }
            
        } else if ( curPos == runA.st ) {
            
            if ( curPos < runB.st ) {
                endA = runA.en <= runB.st;
                startB = runA.en >= runB.st;
                nextPos = endA ? runA.en : runB.st;
            } else if ( curPos == runB.st ) {
                endA = runA.en <= runB.en;
                endB = runA.en >= runB.en;
                nextPos = endA? runA.en : runB.en;
            } else {
                endA = runA.en <= runB.en;
                endB = runA.en >= runB.en;
                nextPos = endA ? runA.en : runB.en;
            }
            
        } else {
            
            if ( curPos < runB.st ) {
                endA = runA.en <= runB.st;
                startB = runA.en >= runB.st;
                nextPos = startB ? runB.st : runA.en;
            } else if ( curPos == runB.st ) {
                endA = runA.en <= runB.en;
                endB = runA.en >= runB.en;
                nextPos = endA ? runA.en : runB.en;
            } else {
                endA = runA.en <= runB.en;
                endB = runA.en >= runB.en;
                nextPos = endA ? runA.en : runB.en;
            }
        }

        float oValA = valA;
        float oValB = valB;
        valA = inA ? ValAt(nextPos, runA.st, runA.en, runA.vst, runA.ven) : 0;
        valB = inB ? ValAt(nextPos, runB.st, runB.en, runB.vst, runB.ven) : 0;
		
        if ( mod == bool_op_union ) {
            
            if ( inA || inB ) {
                AddRun(curPos, nextPos, oValA + oValB, valA + valB);
            }
            
        } else if ( mod == bool_op_inters ) {
            
            if ( inA && inB ) {
                AddRun(curPos, nextPos, oValA * oValB, valA * valB);
            }
            
        } else if ( mod == bool_op_diff ) {
            if ( inA ) {
                AddRun(curPos, nextPos, oValA - oValB, valA - valB);
            }
            
        } else if ( mod == bool_op_symdiff ) {
            if ( inA && !(inB) ) {
                AddRun(curPos, nextPos, oValA - oValB, valA - valB);
            }
            if ( !(inA) && inB ) {
                AddRun(curPos, nextPos, oValB - oValA, valB - valA);
            }
        }

        curPos = nextPos;
        if ( startA ) {
            inA = true;
            valA = runA.vst;
        }
        if ( startB ) {
            inB = true;
            valB = runB.vst;
        }
        if ( endA ) {
            inA = false;
            valA = 0;
            curA++;
            if ( curA < int(a->runs.size()) && a->runs[curA].st == curPos ) {
                valA=a->runs[curA].vst;
            }
        }
        if ( endB ) {
            inB = false;
            valB = 0;
            curB++;
            if ( curB < int(b->runs.size()) && b->runs[curB].st == curPos ) {
                valB = b->runs[curB].vst;
            }
        }
    }

    while ( curA < int(a->runs.size()) ) {
        float_ligne_run runA = a->runs[curA];
        inA = (curPos >= runA.st && curPos < runA.en );
        inB = false;

        bool startA = false;
        bool endA = false;
        if ( curPos < runA.st ) {
            nextPos = runA.st;
            startA = true;
        } else if ( curPos >= runA.st ) {
            nextPos = runA.en;
            endA = true;
        }

        float oValA = valA;
        float oValB = valB;
        valA = inA ? ValAt(nextPos, runA.st, runA.en, runA.vst, runA.ven) : 0;
        valB = 0;

        if ( mod == bool_op_union ) {
            
            if ( inA || inB ) {
                AddRun(curPos, nextPos, oValA + oValB, valA + valB);
            }
            
        } else if ( mod == bool_op_inters ) {
            
            if ( inA && inB ) {
                AddRun(curPos, nextPos, oValA * oValB, valA * valB);
            }
            
        } else if ( mod == bool_op_diff ) {

            if ( inA ) {
                AddRun(curPos, nextPos, oValA - oValB, valA - valB);
            }
            
        } else if ( mod == bool_op_symdiff ) {
            
            if ( inA && !(inB) ) {
                AddRun(curPos, nextPos, oValA - oValB, valA - valB);
            }
            if ( !(inA) && inB ) {
                AddRun(curPos, nextPos, oValB - oValA, valB - valA);
            }
            
        }

        curPos = nextPos;
        if ( startA ) {
            inA = true;
            valA = runA.vst;
        }
        if ( endA ) {
            inA = false;
            valA = 0;
            curA++;
            if ( curA < int(a->runs.size()) && a->runs[curA].st == curPos ) {
                valA=a->runs[curA].vst;
            }
        }
    }
    
    while ( curB < int(b->runs.size()) ) {
        float_ligne_run runB = b->runs[curB];
        inB = ( curPos >= runB.st && curPos < runB.en );
        inA = false;

        bool startB = false;
        bool endB = false;
        if ( curPos < runB.st ) {
            nextPos = runB.st;
            startB = true;
        } else if ( curPos >= runB.st ) { // trivially true?
            nextPos = runB.en;
            endB = true;
        }
        
        float oValA = valA;
        float oValB = valB;
        valB = inB ? ValAt(nextPos, runB.st, runB.en, runB.vst, runB.ven) : 0;
        valA = 0;

        if ( mod == bool_op_union ) {
            
            if ( inA || inB ) {
                AddRun(curPos, nextPos, oValA + oValB, valA + valB);
            }
            
        } else if ( mod == bool_op_inters ) {
            
            if ( inA && inB ) {
                AddRun(curPos, nextPos, oValA * oValB, valA * valB);
            }
            
        } else if ( mod == bool_op_diff ) {

            if ( inA ) {
                AddRun(curPos, nextPos, oValA - oValB, valA - valB);
            }

        } else if ( mod == bool_op_symdiff ) {

            if ( inA && !(inB) ) {
                AddRun(curPos, nextPos, oValA - oValB, valA - valB);
            }
            if ( !(inA) && inB ) {
                AddRun(curPos, nextPos, oValB - oValA, valB - valA);
            }

        }

        curPos = nextPos;
        if ( startB ) {
            inB = true;
            valB = runB.vst;
        }
        
        if ( endB ) {
            inB = false;
            valB = 0;
            curB++;
            if ( curB < int(b->runs.size()) && b->runs[curB].st == curPos ) {
                valB = b->runs[curB].vst;
            }
        }
    }
}


void FloatLigne::Min(FloatLigne *a, float tresh, bool addIt)
{
    Reset();
    if ( a->runs.empty() ) {
        return;
    }

    bool startExists = false;
    float lastStart=0;
    float lastEnd = 0;
    
    for (int i = 0; i < int(a->runs.size()); i++) {
        float_ligne_run runA = a->runs[i];
        if ( runA.vst <= tresh ) {
            if ( runA.ven <= tresh ) {
                if ( startExists ) {
                    if ( lastEnd >= runA.st - 0.00001 ) {
                        lastEnd = runA.en;
                    } else {
                        if ( addIt ) {
                            AddRun(lastStart, lastEnd, tresh, tresh);
                        }
                        lastStart = runA.st;
                        lastEnd = runA.en;
                    }
                } else {
                    lastStart = runA.st;
                    lastEnd = runA.en;
                }
                startExists = true;
            } else {
                float cutPos = (runA.st * (tresh - runA.ven) + runA.en * (runA.vst - tresh)) / (runA.vst - runA.ven);
                if ( startExists ) {
                    if ( lastEnd >= runA.st - 0.00001 ) {
                        if ( addIt ) {
                            AddRun(lastStart, cutPos, tresh, tresh);
                        }
                        AddRun(cutPos,runA.en, tresh, runA.ven);
                    } else {
                        if ( addIt ) {
                            AddRun(lastStart, lastEnd, tresh, tresh);
                        }
                        if ( addIt ) {
                            AddRun(runA.st, cutPos, tresh, tresh);
                        }
                        AddRun(cutPos, runA.en, tresh, runA.ven);
                    }
                } else {
                    if ( addIt ) {
                        AddRun(runA.st, cutPos, tresh, tresh);
                    }
                    AddRun(cutPos, runA.en, tresh, runA.ven);
                }
                startExists = false;
            }
            
        } else {
            
            if ( runA.ven <= tresh ) {
                float cutPos = (runA.st * (runA.ven - tresh) + runA.en * (tresh - runA.vst)) / (runA.ven - runA.vst);
                if ( startExists ) {
                    if ( addIt ) {
                        AddRun(lastStart, lastEnd, tresh, tresh);
                    }
                }
                AddRun(runA.st, cutPos, runA.vst, tresh);
                startExists = true;
                lastStart = cutPos;
                lastEnd = runA.en;
            } else {
                if ( startExists ) {
                    if ( addIt ) {
                        AddRun(lastStart, lastEnd, tresh, tresh);
                    }
                }
                startExists = false;
                AddRun(runA.st, runA.en, runA.vst, runA.ven);
            }
        }
    }
    
    if ( startExists ) {
        if ( addIt ) {
            AddRun(lastStart, lastEnd, tresh, tresh);
        }
    }
}


void FloatLigne::Split(FloatLigne *a, float tresh, FloatLigne *over)
{
    Reset();
    if ( a->runs.empty() ) {
        return;
    }

    for (int i = 0; i < int(a->runs.size()); i++) {
        float_ligne_run runA = a->runs[i];
        if ( runA.vst >= tresh ) {
            if ( runA.ven >= tresh ) {
                if ( over ) {
                    over->AddRun(runA.st, runA.en, runA.vst, runA.ven);
                }
            } else {
                float cutPos = (runA.st * (tresh - runA.ven) + runA.en * (runA.vst - tresh)) / (runA.vst - runA.ven);
                if ( over ) {
                    over->AddRun(runA.st, cutPos, runA.vst, tresh);
                }
                AddRun(cutPos, runA.en, tresh, runA.ven);
            }
        } else {
            if ( runA.ven >= tresh ) {
                float cutPos = (runA.st * (runA.ven - tresh) + runA.en * (tresh-runA.vst)) / (runA.ven - runA.vst);
                AddRun(runA.st, cutPos, runA.vst, tresh);
                if ( over ) {
                    over->AddRun(cutPos, runA.en, tresh, runA.ven);
                }
            } else {
                AddRun(runA.st, runA.en, runA.vst, runA.ven);
            }
        }
    }
}


void FloatLigne::Max(FloatLigne *a, float tresh, bool addIt)
{
    Reset();
    if ( a->runs.empty() <= 0 ) {
        return;
    }

    bool startExists = false;
    float lastStart = 0;
    float lastEnd = 0;
    for (int i = 0; i < int(a->runs.size()); i++) {
        float_ligne_run runA = a->runs[i];
        if ( runA.vst >= tresh ) {
            if ( runA.ven >= tresh ) {
                if ( startExists ) {
                    if ( lastEnd >= runA.st-0.00001 ) {
                        lastEnd = runA.en;
                    } else {
                        if ( addIt ) {
                            AddRun(lastStart,lastEnd,tresh,tresh);
                        }
                        lastStart = runA.st;
                        lastEnd = runA.en;
                    }
                } else {
                    lastStart = runA.st;
                    lastEnd = runA.en;
                }
                startExists = true;
            } else {
                float cutPos = (runA.st * (tresh - runA.ven) + runA.en * (runA.vst - tresh)) / (runA.vst - runA.ven);
                if ( startExists ) {
                    if ( lastEnd >= runA.st-0.00001 ) {
                        if ( addIt ) {
                            AddRun(lastStart, cutPos, tresh, tresh);
                        }
                        AddRun(cutPos, runA.en, tresh, runA.ven);
                    } else {
                        if ( addIt ) {
                            AddRun(lastStart, lastEnd, tresh, tresh);
                        }
                        if ( addIt ) {
                            AddRun(runA.st, cutPos, tresh, tresh);
                        }
                        AddRun(cutPos, runA.en, tresh, runA.ven);
                    }
                } else {
                    if ( addIt ) {
                        AddRun(runA.st, cutPos, tresh, tresh);
                    }
                    AddRun(cutPos, runA.en, tresh, runA.ven);
                }
                startExists = false;
            }
            
        } else {
            
            if ( runA.ven >= tresh ) {
                float cutPos = (runA.st * (runA.ven - tresh) + runA.en * (tresh - runA.vst)) / (runA.ven - runA.vst);
                if ( startExists ) {
                    if ( addIt ) {
                        AddRun(lastStart,lastEnd,tresh,tresh);
                    }
                }
                AddRun(runA.st, cutPos, runA.vst, tresh);
                startExists = true;
                lastStart = cutPos;
                lastEnd = runA.en;
            } else {
                if ( startExists ) {
                    if ( addIt ) {
                        AddRun(lastStart,lastEnd,tresh,tresh);
                    }
                }
                startExists = false;
                AddRun(runA.st, runA.en, runA.vst, runA.ven);
            }
        }
    }
    
    if ( startExists ) {
        if ( addIt ) {
            AddRun(lastStart, lastEnd, tresh, tresh);
        }
    }
}


void FloatLigne::Over(FloatLigne *a, float tresh)
{
    Reset();
    if ( a->runs.empty() ) {
        return;
    }

    bool startExists = false;
    float lastStart = 0;
    float lastEnd = 0;
    
    for (int i = 0; i < int(a->runs.size()); i++) {
        float_ligne_run runA = a->runs[i];
        if ( runA.vst >= tresh ) {
            if ( runA.ven >= tresh ) {
                if ( startExists ) {
                    if ( lastEnd >= runA.st - 0.00001 ) {
                        lastEnd = runA.en;
                    } else {
                        AddRun(lastStart, lastEnd, tresh, tresh);
                        lastStart = runA.st;
                        lastEnd = runA.en;
                    }
                } else {
                    lastStart = runA.st;
                    lastEnd = runA.en;
                }
                startExists = true;
                
            } else {
                
                float cutPos = (runA.st * (tresh - runA.ven) + runA.en * (runA.vst - tresh)) / (runA.vst - runA.ven);
                if ( startExists ) {
                    if ( lastEnd >= runA.st - 0.00001 ) {
                        AddRun(lastStart, cutPos, tresh, tresh);
                    } else {
                        AddRun(lastStart, lastEnd, tresh, tresh);
                        AddRun(runA.st, cutPos, tresh, tresh);
                    }
                } else {
                    AddRun(runA.st, cutPos, tresh, tresh);
                }
                startExists = false;
            }
            
        } else {
            if ( runA.ven >= tresh ) {
                float cutPos = (runA.st * (runA.ven - tresh) + runA.en * (tresh - runA.vst)) / (runA.ven - runA.vst);
                if ( startExists ) {
                    AddRun(lastStart, lastEnd, tresh, tresh);
                }
                startExists = true;
                lastStart = cutPos;
                lastEnd = runA.en;
            } else {
                if ( startExists ) {
                    AddRun(lastStart, lastEnd, tresh, tresh);
                }
                startExists = false;
            }
        }
    }
    
    if ( startExists ) {
        AddRun(lastStart, lastEnd, tresh, tresh);
    }
}

void FloatLigne::Enqueue(int no)
{
    if ( firstAc < 0 ) {
        firstAc = lastAc = no;
        bords[no].prev = bords[no].next = -1;
    } else {
        bords[no].next = -1;
        bords[no].prev = lastAc;
        bords[lastAc].next = no;
        lastAc = no;
    }
}

void FloatLigne::Dequeue(int no)
{
    if ( no == firstAc ) {
        if ( no == lastAc ) {
            firstAc = lastAc = -1;
        } else {
            firstAc = bords[no].next;
        }
    } else if ( no == lastAc ) {
        lastAc = bords[no].prev;
    }
    
    if ( bords[no].prev >= 0 ) {
        bords[bords[no].prev].next = bords[no].next;
    }
    if ( bords[no].next >= 0 ) {
        bords[bords[no].next].prev = bords[no].prev;
    }
    
    bords[no].prev=bords[no].next = -1;
}



/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :


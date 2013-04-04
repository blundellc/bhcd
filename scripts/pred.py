#!/usr/bin/python2.7

import sys
from math import *

# only look at the upper triangluar results.
upper_only = True

def logsumexp(xs):
    mm = max(xs)
    return mm + log(sum((exp(xx-mm) for xx in xs)))

def load(fname):
    truth = {}
    lprob = {}
    with open(fname) as pred:
        for line in pred.readlines():
            src, dst, correct, lp_false, lp_true = line.split(',')
            if upper_only and src < dst:
                continue
            truth[src, dst] = correct == "true"
            lprob[src, dst] = { False: float(lp_false), True: float(lp_true) }
            acc = 1e-8
            #pred = lprob[src,dst][True] > lprob[src,dst][False]
            #pred = truth[src,dst]
            #lprob[src,dst][pred] = log(1.0 - acc)
            #lprob[src,dst][not pred] = log(acc)
            #lprob[src,dst][False] = log(0.5 + acc)
            #lprob[src,dst][True] = log(0.5 - acc)
    return truth, lprob

def blend_preds(truths, lprobs):
    truth0 = truths[0]
    for truth in truths[1:]:
        if truth != truth0:
            raise 'truth mismatch'

    lprob = lprobs[0]
    nn = 1.0
    for lprob1 in lprobs[1:]:
        nn += 1.0
        for src, dst in lprob.keys():
            lprob[src, dst][False] += lprob1[src, dst][False]
            lprob[src, dst][True] += lprob1[src, dst][True]

    for src, dst in lprob.keys():
        lprob[src, dst][False] /= nn
        lprob[src, dst][True] /= nn
    return truth0, lprob

def check_sum(lprob):
    for kk in lprob:
        zz = logsumexp(lprob[kk].values())
        if zz > 1e-5:
            print kk, zz

def mean(xs):
    return sum(xs)/float(len(xs))

def var(xs):
    mm = mean(xs)
    return sum((x*x for x in xs))/float(len(xs)) - mm*mm

def std(xs):
    return sqrt(max(0, var(xs)))

def std_err(xs):
    nn = len(xs)
    return std(xs)/sqrt(nn)

def avg_log_prob(truth, lprob):
    lp = []
    for src, dst in truth:
        lp.append(lprob[src, dst][truth[src, dst]])
    print 'lpred',mean(lp),'+/-',std_err(lp)
#    log2 = log(2.0)
#    lp2 = [-ll/log2 for ll in lp]
#    print 'bits',mean(lp2),'+/-',std_err(lp2)

def loss01(truth, lprob):
    total = 0.0
    wrong = 0.0
    for src, dst in truth:
        if upper_only and src < dst:
            continue
        total += 1.0
        call = max(lprob[src, dst].keys(), key=lambda kk: lprob[src,dst][kk])
        if call != truth[src, dst]:
            wrong += 1.0
    return wrong/total

def rmse(truth, lprob):
    se = []
    for src, dst in truth:
        if upper_only and src < dst:
            continue
        correct = 0.0
        if truth[src, dst]:
            correct = 1.0
        p = exp(lprob[src,dst][True])
        se.append(correct - 2*correct*p + p)
    return sqrt(mean(se))

def auc(truth, lprob):
    # number of links and non-links in truth
    nl = float(sum(truth.values()))
    nn = float(len(truth) - nl)
    assert(nl + nn == len(truth))
    # ri is the rank of the ith positive example
    So = 0.0
    sorted_k = sorted(truth.keys(), key=lambda kk: lprob[kk][True])
    for ii, kk in enumerate(sorted_k):
        if truth[kk]:
            So += ii+1

    return (So - (nl*(nl+1))/2)/(nl*nn)

def main():
    truths = []
    lprobs = []
    for fname in sys.argv[1:]:
        truth, lprob = load(fname)
        truths.append(truth)
        lprobs.append(lprob)

    truth, lprob = blend_preds(truths, lprobs)

    check_sum(lprob)
    avg_log_prob(truth, lprob)
    print 'loss01 %2.5f' % loss01(truth, lprob)
    print 'rmse %2.5f' % rmse(truth, lprob)
    print 'auc %2.5f' % auc(truth, lprob)

if __name__ == '__main__':
    main()

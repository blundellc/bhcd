#!/usr/bin/python2.7

import sys
from math import *

# only look at the upper triangluar results.
upper_only = False

def logsumexp(xs):
    mm = max(xs)
    return mm + log(sum((exp(xx-mm) for xx in xs)))

def load(fname):
    truth = {}
    lprob = {}
    with open(fname) as pred:
        for line in pred.readlines():
            src, dst, correct, lp_false, lp_true = line.split(',')
            if not upper_only or src >= dst:
                truth[src, dst] = correct == "true"
                lprob[src, dst] = { False: float(lp_false), True: float(lp_true) }
                #lprob[src, dst] = { False: log(0.9999), True: log(0.0001) }
    return truth, lprob

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
    return sqrt(var(xs))

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
        total += 1
        call = max(lprob[src, dst].keys(), key=lambda kk: lprob[src,dst][kk])
        if call != truth[src, dst]:
            wrong += 1
    return wrong/total

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
    truth, lprob = load(sys.argv[1])
    check_sum(lprob)
    avg_log_prob(truth, lprob)
    print 'loss01 %2.5f' % loss01(truth, lprob)
    print 'auc %2.5f' % auc(truth, lprob)

if __name__ == '__main__':
    main()

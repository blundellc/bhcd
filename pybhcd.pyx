# distutils: language = c
cdef extern from "bhcd/bhcd/bhcd.h":
    ctypedef struct GRand
    GRand* g_rand_new()

cdef bhcd():
    g_rand_new()
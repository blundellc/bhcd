# distutils: language = c
cdef extern from "bhcd/bhcd/bhcd.h":
    ctypedef struct GRand
    ctypedef struct Tree
    ctypedef struct Dataset
    ctypedef struct Build
    ctypedef struct Params
    ctypedef char gchar
    ctypedef unsigned int guint
    ctypedef int gint
    ctypedef gint gboolean
    GRand* g_rand_new()
    void tree_io_save_string(Tree*, gchar**)
    Build * build_new(GRand *rng, Params * params, guint num_restarts, gboolean sparse)
    Dataset * dataset_gml_load(const gchar*)
    
cpdef bhcd(gml_py_str):
    cdef int a = 1
    cdef double b = <double> a
    cdef char* gml_c_str = gml_py_str
    rng_ptr = g_rand_new()
    dataset_ptr = dataset_gml_load( <gchar*> gml_c_str)
    
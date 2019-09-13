'''this wrapper runs the bhcd executable, with gml files as argument input and read the output tree files
'''
import os
import json
import tempfile
import subprocess
import sys
import csv
import math

import networkx as nx
from ete3 import Tree

BUILD_DIR = os.path.join(os.path.dirname(__file__), 'src', 'build')

def parse_tree(filename):
    st = ''
    with open(filename) as f:
        st = f.read()
    st = st.replace('\n','').replace('\t','').replace('\\','/')
    st = st.replace(']}','')
    st += ']}'
    js = json.loads(st)
    tree = Tree()
    tree.add_features(custom_name='0')
    for i in js["tree"]:
        # parse stem    
        if(i.get("stem")):
            if(i["stem"]["parent"] == 0):
                parent_node = tree
            else:
                stem_parent_name = str(i["stem"]["parent"])
                parent_node = tree.search_nodes(custom_name=stem_parent_name)[0]
            child = parent_node.add_child()
            child.add_features(custom_name=str(i["stem"]["child"]))
        elif(i.get("leaf")):# parse leaf
            leaf_parent_name = str(i["leaf"]["parent"])
            parent_node = tree.search_nodes(custom_name=leaf_parent_name)[0]
            parent_node.add_child(name=str(i["leaf"]["label"]))
    return tree

def parse_predict_file(filename):
    dic = {}
    with open(filename) as f:
        csv_r = csv.reader(f, delimiter=',')
        for r in csv_r:
            index_i = int(r[1])
            index_j = int(r[2])
            prob_true = math.exp(float(r[-1]))
            dic[(index_i, index_j)] = prob_true
    return dic
    
class BHCD:
    def __init__(self, restart=1, gamma=0.4, alpha=1.0, beta=0.2, delta=1.0, _lambda=0.2, sparse=True):
        self.bhcd = 'bhcd'
        if(os.environ.get('BHCD')):
            self.bhcd = os.environ['BHCD']
        else:
            if(sys.platform == 'win32'):
                exe_path = os.path.join(os.path.dirname(__file__), 'src', 'build', 'bhcd', 'Release', 'bhcd.exe')
            else:
                exe_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), 'src', 'build', 'bhcd', 'bhcd')
            if(os.path.exists(exe_path)):
                self.bhcd = exe_path
        self.tree = Tree()
        self._gamma = gamma
        self._alpha = alpha
        self._beta = beta
        self._delta = delta
        self._lambda = _lambda
        self.sparse = sparse
        self.restart = restart
    def _write_gml(self, G):
        '''write to tmp dir
        '''
        _G = nx.Graph()
        for node in G.nodes():
            _G.add_node(node)
        for edge in G.edges():
            i,j = edge
            _G.add_edge(i,j)
        _, filename = tempfile.mkstemp()
        self.gml = filename
        nx.write_gml(_G, filename)

    def _write_gml_test(self, node_num):
        _G = nx.Graph()
        for i in range(node_num):
            for j in range(i+1, node_num):
                _G.add_edge(i, j)
        _, filename = tempfile.mkstemp()
        self.test_gml = filename
        nx.write_gml(_G, filename)

    def fit(self, G, initialize_tree = True, predict=True):
        self._write_gml(G)
        if(predict):
            self.node_size = len(G)
            self._write_gml_test(self.node_size)            
        # write files to build directory, replace the last run of fit
        intermediate_file_prefix = 'runner_%d'%os.getpid()
        command_list = [self.bhcd, '-g', str(self._gamma), '-a', str(self._alpha),
            '-b', str(self._beta), '-d', str(self._delta), '-l', str(self._lambda),
            '-p', intermediate_file_prefix, '-R', str(self.restart), '--data-symmetric']
        if(predict):
            self._write_gml_test(len(G))
            command_list.extend(['-t', self.test_gml])    
        if(self.sparse):
            command_list.append('-S')
        command_list.append(self.gml)
        subprocess.run(command_list, cwd=BUILD_DIR)
        # block until call returned
        tree_filename = os.path.join(BUILD_DIR,  intermediate_file_prefix + '.tree')
        if(initialize_tree):
            self.tree = parse_tree(tree_filename)
        if(predict):
            predict_file = os.path.join(BUILD_DIR, intermediate_file_prefix + '.pred')
            self.predict_dic = parse_predict_file(predict_file)
    
    def predict(self, node_index_i, node_index_j, weight_added = 1):
        if not(type(node_index_i) is int and type(node_index_j) is int):
            raise ValueError("two index should be int typed")
        if not(node_index_i >= 0 and node_index_i < self.node_size and node_index_j >=0 and node_index_j < self.node_size):
            raise IndexError("index out of range")
        if(node_index_i < node_index_j):
            return self.predict_dic[(node_index_i, node_index_j)] > 0.5 
        else:
            return self.predict_dic[(node_index_j, node_index_i)] > 0.5    
            
if __name__ == '__main__':
    G = nx.Graph()
    G.add_edge(0,1)
    G.add_edge(2,3)    
    a = BHCD()
    a.fit(G)
    print(a.tree)

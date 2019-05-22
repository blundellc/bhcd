'''this wrapper runs the bhcd executable, with gml files as argument input and read the output tree files
'''
import os
import json
import tempfile
import subprocess
import sys

import networkx as nx
from ete3 import Tree

BUILD_DIR = os.path.join(os.path.dirname(__file__), 'src', 'build')

def parse_tree(filename):
    st = open(filename).read().replace('\n','').replace('\t','').replace('\\','/')
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

class BHCD:
    def __init__(self, restart=1, gamma=0.4, alpha=1.0, beta=0.2, delta=1.0, _lambda=0.2, sparse=True):
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
        
    def fit(self, G, initialize_tree = True):
        self._write_gml(G)
        # write files to build directory, replace the last run of fit
        command_list = [self.bhcd, '-g', str(self._gamma), '-a', str(self._alpha),
            '-b', str(self._beta), '-d', str(self._delta), '-l', str(self._lambda),
            '-p', 'runner', '-R', str(self.restart), '--data-symmetric']
        if(self.sparse):
            command_list.append('-S')
        command_list.append(self.gml)
        subprocess.run(command_list, cwd=BUILD_DIR)
        # block until call returned
        tree_filename = os.path.join(BUILD_DIR, 'runner.tree')
        if(initialize_tree):
            self.tree = parse_tree(tree_filename)
            
if __name__ == '__main__':
    G = nx.Graph()
    G.add_edge(0,1)
    G.add_edge(2,3)    
    a = BHCD()
    a.fit(G)
    print(a.tree)

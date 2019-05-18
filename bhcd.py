'''this wrapper runs the bhcd executable, with gml files as argument input and read the output tree files
'''
import os
import json
import tempfile
import subprocess
import pdb

import networkx as nx
from ete3 import Tree

BUILD_DIR = os.path.join(os.path.dirname(__file__), 'build')

def parse_tree(filename):
    st = open(filename).read()
    js = json.loads(st)
    tree = Tree()
    for i in js["tree"]:
        # parse stem    
        if(i.get("stem")):
            if(i["stem"]["parent"] == 0):
                parent_node = tree
            else:
                stem_parent_name = str(i["stem"]["parent"])
                parent_node = tree.search_nodes(name=stem_parent_name)[0]
            parent_node.add_child(name=str(i["stem"]["child"]))
        elif(i.get("leaf")):# parse leaf
            leaf_parent_name = str(i["leaf"]["parent"])
            parent_node = tree.search_nodes(name=leaf_parent_name)[0]
            parent_node.add_child(name=str(i["leaf"]["label"]))
    return tree

class BHCD:
    def __init__(self):
        if(os.environ.get('BHCD')):
            self.bhcd = os.environ['BHCD']
        else:
            self.bhcd = "bhcd"
        self.tree = Tree()

    def _write_gml(self, G):
        '''write to tmp dir
        '''
        _G = nx.Graph()
        for edge in G.edges():
            i,j = edge
            _G.add_edge(i,j)
        _, filename = tempfile.mkstemp()
        self.gml = filename
        nx.write_gml(_G, filename)
        
    def fit(self, G, initialize_tree = True):
        self._write_gml(G)
        # write files to build directory, replace the last run of fit
        subprocess.run([self.bhcd, '-S', '-p', 'runner', self.gml], cwd=BUILD_DIR)
        # block until call returned
        tree_filename = os.path.join(BUILD_DIR, 'runner.tree')
        if(initialize_tree and self.tree.is_leaf()):
            self.tree = parse_tree(tree_filename)
            
if __name__ == '__main__':
    G = nx.Graph()
    G.add_edge(0,1)
    G.add_edge(2,3)    
    a = BHCD()
    a.fit(G)
    print(a.tree)

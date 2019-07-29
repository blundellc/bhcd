import unittest

import networkx as nx

from bhcd import BHCD

class TestBHCD(unittest.TestCase):
    def test_predict(self):
        G = nx.Graph()
        G.add_edge(0,1)
        G.add_edge(2,3)    
        a = BHCD()
        a.fit(G)
        self.assertTrue(a.predict(0, 1))
        self.assertTrue(a.predict(2, 3))
        self.assertFalse(a.predict(0, 2))
        self.assertFalse(a.predict(3, 0))
        self.assertFalse(a.predict(1, 2))
        self.assertFalse(a.predict(1, 3))
        
if __name__ == '__main__':
    unittest.main()
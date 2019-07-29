# only support edit install
# pip install -e .
from setuptools import setup

with open("README.md") as fh:
    long_description = fh.read()
    
if __name__ == '__main__':
    setup(name = 'bhcd',
          version = '0.1',
          description = 'Bayesian Hierarchical Community Discovery',
          author = 'blundellc, zhaofeng-shu33',
          author_email = '616545598@qq.com',
          url = 'https://github.com/zhaofeng-shu33/bhcd',
          maintainer = 'zhaofeng-shu33',
          maintainer_email = '616545598@qq.com',
          long_description = long_description,
          long_description_content_type="text/markdown",          
          install_requires = ['networkx'],
          license = 'Apache License Version 2.0',
          py_modules = ['bhcd'],
          classifiers = (
              "Development Status :: 4 - Beta",
              "Programming Language :: Python :: 3.7",
              "Programming Language :: Python :: 3.6",
              "Operating System :: OS Independent",
          ),
    )
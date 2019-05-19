# bhcd: Bayesian Hierarchical Community Discovery

An efficient Bayesian nonparametric model for discovering hierarchical community structure in social networks. 

## Build

Use CMake to build, necessary dependencies are glib and gsl. For Windows, you can use [vcpkg](https://github.com/microsoft/vcpkg) to install the dependencies.
remove lua support.

```sh
# apt install automake libtool libgsl-dev liblua5.1-dev
$ autoreconf -i
$ ./scripts/config release
```

## Usage

Python wrapper. Set an environment variable called `BHCD`, pointing to the executable of bhcd. Then
you can run `python bhcd.py` to get the hierachical tree.

```sh
$ cat >tiny.gml <<EOF
graph [
    sparse 0
    node [ id 0 label "a" ]
    node [ id 1 label "b" ]
    node [ id 2 label "c" ]
    edge [ source 0 target 1 weight 1 ]
    edge [ source 1 target 0 weight 1 ]
]
EOF
$ ./src/bhcd/bhcd -S -p tiny tiny.gml
seed: 2a23b6bb
output prefix: tiny
time: 3.010000e-04s
tree: logprob: -1.92e+00 (0:4,1:2/0:4,1:0) #intern: 2 -1.92e+00:{-1.03e+00:{a , b }, c }
$ ./scripts/bhcd_plot tiny.tree tiny.fit tiny.pdf
-4.82e+00:{-1.66e-01:{a, b}, c}
```
Then tiny.pdf will have a plot of the clustering.

## Paper

Blundell, C., & Teh, Y. W. (2013). Bayesian hierarchical community discovery. In Advances in Neural Information Processing Systems (pp. 1601-1609).

```bibtex
@inproceedings{blundell2013bayesian,
  title={Bayesian hierarchical community discovery},
  author={Blundell, Charles and Teh, Yee Whye},
  booktitle={Advances in Neural Information Processing Systems},
  pages={1601--1609},
  year={2013}
}
```

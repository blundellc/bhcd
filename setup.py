import os
import sys
from setuptools import setup, Extension
from subprocess import Popen, PIPE
from shutil import copyfile

from Cython.Build import cythonize

with open("README.md") as fh:
    long_description = fh.read()

def set_up_git_version():
    if(os.path.exists('git_version.h')):
        return
    command_list = ['git', 'rev-parse', '--verify', 'HEAD']
    working_dir = os.path.join(os.path.dirname(__file__), 'bhcd')
    p = Popen(command_list, cwd=working_dir, stdout=PIPE)
    outs, _ = p.communicate()
    hash_str = outs.decode('ascii')
    version_macro = '#define GIT_VERSION %s' % hash_str[:7]
    with open('git_version.h', 'w') as f:
        f.write(version_macro)

def find_all_c(given_dir, exclude=[]):
    c_file_list = []
    for i in os.listdir(given_dir):
        if i.find('.c') > 0 and exclude.count(i) == 0:
            c_file_list.append(os.path.join(given_dir, i))
    return c_file_list

def set_up_glib_include_path_linux(extra_include_path):
    GLIB_ROOT = os.environ.get('GLIB_ROOT', '/usr/')
    glib_platform_indepedent_include_path = os.path.join(GLIB_ROOT, 'include', 'glib-2.0')
    glib_platform_depedent_include_path = os.path.join(GLIB_ROOT, 'lib64', 'glib-2.0', 'include')
    extra_include_path.append(glib_platform_indepedent_include_path)
    extra_include_path.append(glib_platform_depedent_include_path)
        	
def set_up_cython_extension():
    extra_include_path = []
    extra_include_path.append(os.path.join(os.getcwd()))
    extra_include_path.append(os.path.join(os.getcwd(), 'bhcd', 'bhcd'))
    extra_include_path.append(os.path.join(os.getcwd(), 'bhcd', 'hccd'))

    extra_lib_dir = []

    if os.environ.get('VCPKG_ROOT'):
        root_dir = os.environ['VCPKG_ROOT']
        triplet = os.environ.get('VCPKG_DEFAULT_TRIPLET', 'x64-windows-bhcd')
        include_dir = os.path.join(root_dir, 'installed', triplet, 'include')
        if os.path.exists(include_dir):
            extra_include_path.append(include_dir)
        lib_dir = os.path.join(root_dir, 'installed', triplet, 'lib')
        if os.path.exists(lib_dir):
            extra_lib_dir.append(lib_dir)

        gsl_run_time_name = 'glib-2.dll'  
        gsl_run_time = os.path.join(root_dir, 'installed', triplet, 'bin', gsl_run_time_name)
        # copy to current directory
        copyfile(gsl_run_time, os.path.join(os.getcwd(), gsl_run_time_name))

    if sys.platform == 'linux':
        set_up_glib_include_path_linux(extra_include_path)    

    # collect library
    sourcefiles = ['pybhcd.pyx']
    sourcefiles.extend(find_all_c(os.path.join(os.getcwd(), 'bhcd', 'bhcd'), exclude=['pagerank.c', 'loadgml.c', 'benchbhcd.c', 'bhcd.c', 'test.c', 'test_bitset_hash.c']))
    sourcefiles.extend(find_all_c(os.path.join(os.getcwd(), 'bhcd', 'hccd'), exclude=[]))
    extra_compile_flags_list = []
    extra_link_flags_list = []    
    if sys.platform == 'win32' and os.environ.get('BHCD_DEBUG'): 
        extra_compile_flags_list.extend(['/Zi', '/Od'])
        extra_link_flags_list.append('/DEBUG')
    extensions = [
        Extension('pybhcd', sourcefiles,
                  include_dirs=extra_include_path,
                  library_dirs=extra_lib_dir,
                  extra_compile_args=extra_compile_flags_list,
                  extra_link_args=extra_link_flags_list,
                  libraries=['glib-2.0', 'gsl']
                 )
    ]
    return cythonize(extensions)

EXT_MODULE_CLASS = set_up_cython_extension()
    
if __name__ == '__main__':
    set_up_git_version()
    setup(name = 'pybhcd',
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
          ext_modules=EXT_MODULE_CLASS,
          classifiers = (
              "Development Status :: 4 - Beta",
              "Programming Language :: Python :: 3.7",
              "Programming Language :: Python :: 3.6",
              "Operating System :: OS Independent",
          ),
    )

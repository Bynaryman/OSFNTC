import os
import re
import sys
import importlib

# Minimum version, enforced by sphinx
needs_sphinx = '3.2.0'


# This is a nasty hack to use platform-agnostic names for types in the
# documentation.

# must be kept alive to hold the patched names
_name_cache = {}

def replace_scalar_type_names():
    """ Rename numpy types to use the canonical names to make sphinx behave """
    import ctypes

    Py_ssize_t = ctypes.c_int64 if ctypes.sizeof(ctypes.c_void_p) == 8 else ctypes.c_int32

    class PyObject(ctypes.Structure):
        pass

    class PyTypeObject(ctypes.Structure):
        pass

    PyObject._fields_ = [
        ('ob_refcnt', Py_ssize_t),
        ('ob_type', ctypes.POINTER(PyTypeObject)),
    ]


    PyTypeObject._fields_ = [
        # varhead
        ('ob_base', PyObject),
        ('ob_size', Py_ssize_t),
        # declaration
        ('tp_name', ctypes.c_char_p),
    ]

    # prevent numpy attaching docstrings to the scalar types
    assert 'numpy.core._add_newdocs_scalars' not in sys.modules
    sys.modules['numpy.core._add_newdocs_scalars'] = object()

    import numpy

    # change the __name__ of the scalar types
    for name in [
        'byte', 'short', 'intc', 'int_', 'longlong',
        'ubyte', 'ushort', 'uintc', 'uint', 'ulonglong',
        'half', 'single', 'double', 'longdouble',
        'half', 'csingle', 'cdouble', 'clongdouble',
    ]:
        typ = getattr(numpy, name)
        c_typ = PyTypeObject.from_address(id(typ))
        c_typ.tp_name = _name_cache[typ] = b"numpy." + name.encode('utf8')

    # now generate the docstrings as usual
    del sys.modules['numpy.core._add_newdocs_scalars']
    import numpy.core._add_newdocs_scalars

replace_scalar_type_names()

# -----------------------------------------------------------------------------
# General configuration
# -----------------------------------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be extensions
# coming with Sphinx (named 'sphinx.ext.*') or your custom ones.

sys.path.insert(0, os.path.abspath('../sphinxext'))

extensions = [
    'sphinx.ext.autodoc',
    'numpydoc',
    'sphinx.ext.intersphinx',
    'sphinx.ext.coverage',
    'sphinx.ext.doctest',
    'sphinx.ext.autosummary',
    'sphinx.ext.graphviz',
    'sphinx.ext.ifconfig',
    'matplotlib.sphinxext.plot_directive',
    'IPython.sphinxext.ipython_console_highlighting',
    'IPython.sphinxext.ipython_directive',
    'sphinx.ext.mathjax',
    'sphinx_panels',
]

skippable_extensions = [
    ('breathe', 'skip generating C/C++ API from comment blocks.'),
]
for ext, warn in skippable_extensions:
    ext_exist = importlib.util.find_spec(ext) is not None
    if ext_exist:
        extensions.append(ext)
    else:
        print(f"Unable to find Sphinx extension '{ext}', {warn}.")

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# The suffix of source filenames.
source_suffix = '.rst'

# Will change to `root_doc` in Sphinx 4
master_doc = 'index'

# General substitutions.
project = 'NumPy'
copyright = '2008-2022, NumPy Developers'

# The default replacements for |version| and |release|, also used in various
# other places throughout the built documents.
#
import numpy
# The short X.Y version (including .devXXXX, rcX, b1 suffixes if present)
version = re.sub(r'(\d+\.\d+)\.\d+(.*)', r'\1\2', numpy.__version__)
version = re.sub(r'(\.dev\d+).*?$', r'\1', version)
# The full version, including alpha/beta/rc tags.
release = numpy.__version__
print("%s %s" % (version, release))

# There are two options for replacing |today|: either, you set today to some
# non-false value, then it is used:
#today = ''
# Else, today_fmt is used as the format for a strftime call.
today_fmt = '%B %d, %Y'

# List of documents that shouldn't be included in the build.
#unused_docs = []

# The reST default role (used for this markup: `text`) to use for all documents.
default_role = "autolink"

# List of directories, relative to source directories, that shouldn't be searched
# for source files.
exclude_dirs = []

# If true, '()' will be appended to :func: etc. cross-reference text.
add_function_parentheses = False

# If true, the current module name will be prepended to all description
# unit titles (such as .. function::).
#add_module_names = True

# If true, sectionauthor and moduleauthor directives will be shown in the
# output. They are ignored by default.
#show_authors = False

def setup(app):
    # add a config value for `ifconfig` directives
    app.add_config_value('python_version_major', str(sys.version_info.major), 'env')
    app.add_lexer('NumPyC', NumPyLexer)

# While these objects do have type `module`, the names are aliases for modules
# elsewhere. Sphinx does not support referring to modules by an aliases name,
# so we make the alias look like a "real" module for it.
# If we deemed it desirable, we could in future make these real modules, which
# would make `from numpy.char import split` work.
sys.modules['numpy.char'] = numpy.char
sys.modules['numpy.testing.dec'] = numpy.testing.dec

# -----------------------------------------------------------------------------
# HTML output
# -----------------------------------------------------------------------------

html_theme = 'pydata_sphinx_theme'

html_logo = '_static/numpylogo.svg'

html_favicon = '_static/favicon/favicon.ico'

html_theme_options = {
  "logo_link": "index",
  "github_url": "https://github.com/numpy/numpy",
  "twitter_url": "https://twitter.com/numpy_team",
  "collapse_navigation": True,
  "external_links": [
      {"name": "Learn", "url": "https://numpy.org/numpy-tutorials/"}
      ],
}

html_title = "%s v%s Manual" % (project, version)
html_static_path = ['_static']
html_last_updated_fmt = '%b %d, %Y'

html_use_modindex = True
html_copy_source = False
html_domain_indices = False
html_file_suffix = '.html'

htmlhelp_basename = 'numpy'

if 'sphinx.ext.pngmath' in extensions:
    pngmath_use_preview = True
    pngmath_dvipng_args = ['-gamma', '1.5', '-D', '96', '-bg', 'Transparent']

mathjax_path = "scipy-mathjax/MathJax.js?config=scipy-mathjax"

plot_html_show_formats = False
plot_html_show_source_link = False

# -----------------------------------------------------------------------------
# LaTeX output
# -----------------------------------------------------------------------------

# The paper size ('letter' or 'a4').
#latex_paper_size = 'letter'

# The font size ('10pt', '11pt' or '12pt').
#latex_font_size = '10pt'

# XeLaTeX for better support of unicode characters
latex_engine = 'xelatex'

# Grouping the document tree into LaTeX files. List of tuples
# (source start file, target name, title, author, document class [howto/manual]).
_stdauthor = 'Written by the NumPy community'
latex_documents = [
  ('reference/index', 'numpy-ref.tex', 'NumPy Reference',
   _stdauthor, 'manual'),
  ('user/index', 'numpy-user.tex', 'NumPy User Guide',
   _stdauthor, 'manual'),
]

# The name of an image file (relative to this directory) to place at the top of
# the title page.
#latex_logo = None

# For "manual" documents, if this is true, then toplevel headings are parts,
# not chapters.
#latex_use_parts = False

latex_elements = {
    'fontenc': r'\usepackage[LGR,T1]{fontenc}'
}

# Additional stuff for the LaTeX preamble.
latex_elements['preamble'] = r'''
% In the parameters section, place a newline after the Parameters
% header
\usepackage{xcolor}
\usepackage{expdlist}
\let\latexdescription=\description
\def\description{\latexdescription{}{} \breaklabel}
% but expdlist old LaTeX package requires fixes:
% 1) remove extra space
\usepackage{etoolbox}
\makeatletter
\patchcmd\@item{{\@breaklabel} }{{\@breaklabel}}{}{}
\makeatother
% 2) fix bug in expdlist's way of breaking the line after long item label
\makeatletter
\def\breaklabel{%
    \def\@breaklabel{%
        \leavevmode\par
        % now a hack because Sphinx inserts \leavevmode after term node
        \def\leavevmode{\def\leavevmode{\unhbox\voidb@x}}%
    }%
}
\makeatother

% Make Examples/etc section headers smaller and more compact
\makeatletter
\titleformat{\paragraph}{\normalsize\py@HeaderFamily}%
            {\py@TitleColor}{0em}{\py@TitleColor}{\py@NormalColor}
\titlespacing*{\paragraph}{0pt}{1ex}{0pt}
\makeatother

% Fix footer/header
\renewcommand{\chaptermark}[1]{\markboth{\MakeUppercase{\thechapter.\ #1}}{}}
\renewcommand{\sectionmark}[1]{\markright{\MakeUppercase{\thesection.\ #1}}}
'''

# Documents to append as an appendix to all manuals.
#latex_appendices = []

# If false, no module index is generated.
latex_use_modindex = False


# -----------------------------------------------------------------------------
# Texinfo output
# -----------------------------------------------------------------------------

texinfo_documents = [
  ("contents", 'numpy', 'NumPy Documentation', _stdauthor, 'NumPy',
   "NumPy: array processing for numbers, strings, records, and objects.",
   'Programming',
   1),
]


# -----------------------------------------------------------------------------
# Intersphinx configuration
# -----------------------------------------------------------------------------
intersphinx_mapping = {
    'neps': ('https://numpy.org/neps', None),
    'python': ('https://docs.python.org/dev', None),
    'scipy': ('https://docs.scipy.org/doc/scipy/reference', None),
    'matplotlib': ('https://matplotlib.org/stable', None),
    'imageio': ('https://imageio.readthedocs.io/en/stable', None),
    'skimage': ('https://scikit-image.org/docs/stable', None),
    'pandas': ('https://pandas.pydata.org/pandas-docs/stable', None),
    'scipy-lecture-notes': ('https://scipy-lectures.org', None),
    'pytest': ('https://docs.pytest.org/en/stable', None),
    'numpy-tutorials': ('https://numpy.org/numpy-tutorials', None),
    'numpydoc': ('https://numpydoc.readthedocs.io/en/latest', None),
}


# -----------------------------------------------------------------------------
# NumPy extensions
# -----------------------------------------------------------------------------

# If we want to do a phantom import from an XML file for all autodocs
phantom_import_file = 'dump.xml'

# Make numpydoc to generate plots for example sections
numpydoc_use_plots = True

# -----------------------------------------------------------------------------
# Autosummary
# -----------------------------------------------------------------------------

autosummary_generate = True

# -----------------------------------------------------------------------------
# Coverage checker
# -----------------------------------------------------------------------------
coverage_ignore_modules = r"""
    """.split()
coverage_ignore_functions = r"""
    test($|_) (some|all)true bitwise_not cumproduct pkgload
    generic\.
    """.split()
coverage_ignore_classes = r"""
    """.split()

coverage_c_path = []
coverage_c_regexes = {}
coverage_ignore_c_items = {}


# -----------------------------------------------------------------------------
# Plots
# -----------------------------------------------------------------------------
plot_pre_code = """
import numpy as np
np.random.seed(0)
"""
plot_include_source = True
plot_formats = [('png', 100), 'pdf']

import math
phi = (math.sqrt(5) + 1)/2

plot_rcparams = {
    'font.size': 8,
    'axes.titlesize': 8,
    'axes.labelsize': 8,
    'xtick.labelsize': 8,
    'ytick.labelsize': 8,
    'legend.fontsize': 8,
    'figure.figsize': (3*phi, 3),
    'figure.subplot.bottom': 0.2,
    'figure.subplot.left': 0.2,
    'figure.subplot.right': 0.9,
    'figure.subplot.top': 0.85,
    'figure.subplot.wspace': 0.4,
    'text.usetex': False,
}


# -----------------------------------------------------------------------------
# Source code links
# -----------------------------------------------------------------------------

import inspect
from os.path import relpath, dirname

for name in ['sphinx.ext.linkcode', 'numpydoc.linkcode']:
    try:
        __import__(name)
        extensions.append(name)
        break
    except ImportError:
        pass
else:
    print("NOTE: linkcode extension not found -- no links to source generated")


def _get_c_source_file(obj):
    if issubclass(obj, numpy.generic):
        return r"core/src/multiarray/scalartypes.c.src"
    elif obj is numpy.ndarray:
        return r"core/src/multiarray/arrayobject.c"
    else:
        # todo: come up with a better way to generate these
        return None


def linkcode_resolve(domain, info):
    """
    Determine the URL corresponding to Python object
    """
    if domain != 'py':
        return None

    modname = info['module']
    fullname = info['fullname']

    submod = sys.modules.get(modname)
    if submod is None:
        return None

    obj = submod
    for part in fullname.split('.'):
        try:
            obj = getattr(obj, part)
        except Exception:
            return None

    # strip decorators, which would resolve to the source of the decorator
    # possibly an upstream bug in getsourcefile, bpo-1764286
    try:
        unwrap = inspect.unwrap
    except AttributeError:
        pass
    else:
        obj = unwrap(obj)

    fn = None
    lineno = None

    # Make a poor effort at linking C extension types
    if isinstance(obj, type) and obj.__module__ == 'numpy':
        fn = _get_c_source_file(obj)

    if fn is None:
        try:
            fn = inspect.getsourcefile(obj)
        except Exception:
            fn = None
        if not fn:
            return None

        # Ignore re-exports as their source files are not within the numpy repo
        module = inspect.getmodule(obj)
        if module is not None and not module.__name__.startswith("numpy"):
            return None

        try:
            source, lineno = inspect.getsourcelines(obj)
        except Exception:
            lineno = None

        fn = relpath(fn, start=dirname(numpy.__file__))

    if lineno:
        linespec = "#L%d-L%d" % (lineno, lineno + len(source) - 1)
    else:
        linespec = ""

    if 'dev' in numpy.__version__:
        return "https://github.com/numpy/numpy/blob/main/numpy/%s%s" % (
           fn, linespec)
    else:
        return "https://github.com/numpy/numpy/blob/v%s/numpy/%s%s" % (
           numpy.__version__, fn, linespec)

from pygments.lexers import CLexer
from pygments.lexer import inherit, bygroups
from pygments.token import Comment

class NumPyLexer(CLexer):
    name = 'NUMPYLEXER'

    tokens = {
        'statements': [
            (r'@[a-zA-Z_]*@', Comment.Preproc, 'macro'),
            inherit,
        ],
    }


# -----------------------------------------------------------------------------
# Breathe & Doxygen
# -----------------------------------------------------------------------------
breathe_projects = dict(numpy=os.path.join("..", "build", "doxygen", "xml"))
breathe_default_project = "numpy"
breathe_default_members = ("members", "undoc-members", "protected-members")

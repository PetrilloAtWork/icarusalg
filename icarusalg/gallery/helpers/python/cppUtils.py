#!/usr/bin/env python

from __future__ import print_function

__doc__ = """
Collection of utilities to interface C++ code with Python via PyROOT.

This module requires ROOT.
"""

__all__ = [
  'readHeader',
  'SourceCode',
  ]

import sys, os
from ROOTutils import ROOT


################################################################################
def readHeader(headerPath):
    """Make the ROOT C++ jit compiler read the specified header."""
    ROOT.gROOT.ProcessLine('#include "%s"' % headerPath)
# readHeader()


################################################################################
class SourceCentral:
  """
  A class keeping track of the sources and where to look for them.
  
  A global instance of it is provided with the name `SourceCode`
  (which will be used in the examples).
  
  An object of this class interfaces with ROOT (PyROOT), which provides the
  actual functionality. This also means that multiple instances of
  `SourceCentral` all share the same ROOT backend.
  
  
  How to load a C++ class into Python
  ====================================
  
  The misleading title notwithstanding, this paragraph shows how to load a
  C++ _library_ so that its content is accessible via `ROOT`.
  The idea is that both a C++ header needs to be loaded, and also the
  compiled library. And when a header is loaded, its dependencies must already
  be known.
  
  Function `SourceCentral.load()` attempts to load into ROOT either a C++ header
  file or a library (the decision is based on the name suffix).
  That is a gateway to the functions `loadHeader()` and `loadLibrary()`, plus
  `loadHeaderFromUPS()` which performs lookup on the assumption of the header
  belonging to a UPS product; a call `SourceCentral.loadHeader()` does not use
  such assumptions, and it has to rely on the include paths registered in the
  object with `addIncPath()`, `addIncPathEnv()` and similar.
  The equivalent function for libraries, `loadLibrary()`, instead, only looks
  into the standard library path from the operating system, but it allows to
  specify extra lookup directories on each call.
  
  Helper functions `findHeader()` and `findLibrary()` may help figuring out the
  right paths and keywords to specify to load a header or library.
  
  
  Example: load LArSoft's `geo::PlaneID` class
  ---------------------------------------------
  
  The essential information we need is that such class is defined in header
  `larcoreobj/SimpleTypesAndConstants/geo_types.h`, and its library is
  `larcoreobj_SimpleTypesAndConstants`
  (i.e. `liblarcoreobj_SimpleTypesAndConstants.so`).
  
  Thus, run:
      
      import cppUtils, ROOT
      cppUtils.SourceCode.load("larcoreobj/SimpleTypesAndConstants/geo_types.h")
      cppUtils.SourceCode.load("larcoreobj_SimpleTypesAndConstants")
      planeID = ROOT.geo.PlaneID(1, 2, 3)
      print(planeID)
      
  will load the two things. The first call uses under the hood
  `SourceCode.loadHeaderFromUPS()`, which under the assumption that the header
  is in a UPS product following the UPS standards, finds it in `$LARCOREOBJ_INC`
  directory. The second call looks for the library in the `LD_LIBRARY_PATH`
  (or equivalent `DYLD_LIBRARY_PATH`). The return code `0` is the way ROOT says
  everything is fine.
  That is enough for us to create and use `geo::PlaneID` objects via ROOT (which
  is why we need to `import ROOT`) as `ROOT.geo.PlaneID`.
  Note that `print()` emits a string with the generic object representation:
  `<cppyy.gbl.geo.PlaneID object at 0x...>`. Specifically for this class, we
  can bind the `geo::PlaneID::toString()` method to Python's standard `__str__`
  attribute with:
      
      ROOT.geo.PlaneID.__str__ = ROOT.geo.PlaneID.toString
      print(planeID)
      
  which will print the familiar `C:1 T:2 P:3` format. This binding and a few
  others are performed by geometry loading utilities in `LArSoftUtils` module.
  
  
  """
  AllPlatformInfo = {
    'Linux': {
      'Name':       'Linux',
      'LibSuffix':  '.so',
      'LibEnvPath': 'LD_LIBRARY_PATH',
    },
    'Darwin': {
      'Name':       'Darwin',
      'LibSuffix':  '.dylib',
      'LibEnvPath': 'DYLD_LIBRARY_PATH', # might be not honoured
    },
  } # AllPlatformInfo
  PlatformInfo = AllPlatformInfo[os.uname()[0]]
  
  def __init__(self, *includePaths): # TODO
    self.headers = {}
    self.libraries = {}
    self.includePaths = []
    self.addIncPaths(*includePaths)
    # for
  # __init__()
  
  def addIncPath(self,
   path: "path to be added to the include directory list (${VAR} are expanded)",
   force: "add the path even if already in the list" = False,
   ):
    """
    Adds the specified `path` at the end of the list of paths used for header lookup.
    
    The `path` argument is expanded replacing strings in the form `${VARNAME}`
    with the content of the environment variable `VARNAME` (e.g. in
    `${LARCOREALG_INC}/Geometry` the content of `LARCOREALG_INC` is expanded).
    
    If `path` is already present in the list, it is not added unless `force` is
    set to `True`.
    
    If the path does not exist or is not a directory, a warning is printed, but
    the path is added nevertheless.
    """
    expPath = os.path.expandvars(path)
    if not os.path.isdir(expPath):
      print(
        "Warning: include path '%s'" % path,
        (" ( => '%s')" % expPath if path != expPath else ""),
        " does not exist.",
        sep='',
        file=sys.stderr
        )
    if force or expPath not in self.includePaths:
      self.includePaths.append(expPath)
  # addIncPath()
  
  def addIncPathEnv(self,
   varName: "name of the environment variable holding the single path to be added",
   force: "add the path even if already in the list" = False,
   ):
    """
    Uses `addIncPath()` to add a path to the list of paths for header lookup.
    
    The variable in `varName` must exist (`KeyError` is raised if environment
    does not have such variable), and the content must represent a single path
    (not, for example, a colon-separated sequence of paths).
    
    For more details, see `addIncPath()`.
    """
    self.addIncPath(os.environ[varName], force=force)
  # addIncPathEnv()
  
  def addIncPaths(self, *paths: "paths to be added (one path per argument)"):
    """
    Uses `addIncPath()` to add multiple paths to the list of paths for header lookup.
    
    Each element in `paths` represents a single path to be added.
    
    For more details, see `addIncPath()`.
    """
    for path in paths: self.addIncPath(path)
  # addIncPaths()

  def addIncPathEnvs(self,
   *varNames: "names of environment variables each holding a path to be added",
   ):
    """
    Uses `addIncPath()` to add multiple paths to the list of paths for header lookup.
    
    Each element in `varNames` is an environment variable holding a single path
    to be added. If any of the variables in the list does not exist, an exception
    is raised (`KeyError`), and it is not guaranteed that any of the other
    paths have been (or have not been) added.
    
    For more details, see `addIncPath()`.
    """
    self.addIncPaths(*map((lambda varName: os.environ[varName]), varNames))
  # addIncPathEnvs()

  def find(self,
   relPath: "path to search",
   extraPaths: "optional extra directories" = [],
   ):
    """
    Returns the full path of the object specified by `relPath`.
    
    That object may be a library or not (in which case, a C++ header is assumed
    instead).
    
    This function just dispatch between `findHeader()` and `findLibrary()`
    according to the outcome of `isLibrary(relPath)`.
    """
    return self.findLibrary(relPath, extraPaths=extraPaths) if self.isLibrary(relPath) else self.findHeader(relPath, extraPaths=extraPaths)
  # find()
  
  def findLibrary(self,
   libName: "name of the library to look for",
   extraPaths: "extra lookup directories, one per list entry" = []
   ) -> "path of the library file if found, `None` otherwise":
    """
    The specified library is searched, first in `extraPaths` (if specified)
    and then in the standard OS library path.
    Each entry in `extraPaths` represents a single library path (not, e.g.,
    a colon-separated list of paths).
    
    Environment variable placeholders (`${VARNAME}`) are expanded on each
    extra path (e.g. `"${LARCOREALG_LIB}"` would be replaced by the value of
    the environment variable `LARCOREALG_LIB`).
    
    """
    expLibName = SourceCentral.expandLibraryName(libName)
    for path in reversed(
     SourceCentral.LibraryPaths() + list(map(os.path.expandvars, extraPaths))
     ):
      candidate = os.path.join(path, expLibName)
      if os.path.exists(candidate): return candidate
    else: return None
  # findLibrary()
  
  def findHeader(self,
   relPath: "path to search",
   extraPaths: "optional extra directories for header lookup" = [],
   ) -> "path of the header file if found, `None` otherwise":
    """
    The specified C++ header is searched, first in `extraPaths` (if specified)
    and then in the already registered include paths (see `addIncPath()`).
    
    Each entry in `extraPaths` represents a single path (not, e.g.,
    a colon-separated list of paths).
    
    Environment variable placeholders (`${VARNAME}`) are expanded on each
    extra path (e.g. `"${LARCOREALG_INC}/Geometry"` would be replaced to include
    the value of the environment variable `LARCOREALG_INC`).
    
    """
    for path in reversed(self.includePaths + list(map(os.path.expandvars, extraPaths))):
      candidate = os.path.join(path, relPath)
      if os.path.exists(candidate): return candidate
    else: return None
  # findHeader()
  
  def loadLibrary(self,
   relPath: "library to be loaded (with optional path)",
   extraPaths: "optional extra directories for library lookup" = [],
   force = False,
   ) -> "code from `TSystem::Load()` (0 on success)":
    """
    Makes ROOT load the specified library.
    
    The library specification (`relPath`) is preprocessed with
    `expandLibraryName()` and then passed to ROOT for loading.
    No check is performed on whether it exists.
    
    If the loading is reported successful, the original path (`relPath`) is
    recorded as loaded.
    
    TODO it seems like some logic is missing here to support `extraPaths` and
    skipping to load a known library again.
    """
    expandedName = self.expandLibraryName(relPath)
    res = ROOT.gSystem.Load(expandedName)
    if res == 0: self.libraries[relPath] = expandedName
    return res
  # loadLibrary()
  
  def loadHeader(self,
   headerRelPath: "header to be loaded",
   extraPaths: "optional extra directories for header lookup" = [],
   force: "(unused)" = False,
   ) -> "path of the header loaded":
    """
    Makes ROOT load the specified header.
    
    If the header has already been successfully loaded by this object,
    no loading is performed.
    
    The header path (`headerRelPath`) is located with `findHeader()` (using
    the `extraPaths` first) and then read. Note that currently there is no
    provision to discover whether ROOT was successful in reading the header.
    
    :raises RuntimeError: if the header can't be found.
    """
    try: return self.headers[headerRelPath]
    except KeyError: pass
    headerPath = self.findHeader(headerRelPath, extraPaths=extraPaths)
    if not headerPath: raise RuntimeError("Can't locate header file '%s'" % headerRelPath)
    readHeader(headerPath)
    self.headers[headerRelPath] = headerPath
    return headerPath
  # loadHeader()
  
  
  def loadHeaderFromUPS(self,
   headerRelPath: "header to be loaded",
   extraPaths: "optional extra directories for header lookup" = [],
   force: "(unused)" = False,
   ) -> "path of the header loaded":
    """
    Loads a C++ header from a UPS product.
    
    Assumptions:
    * the specified relative path of the header is under the include directory
      of its UPS product
    * the include directory path is set in a environment variable named with
      the standard UPS pattern (`PRODUCTNAME_INC`)
    * the header relative path starts with a directory that reflects the name
      of the UPS product, `productname/relative/package/path/header.h`
    
    For example, for a `headerRelPath` of `larcorealg/Geometry/GeometryCore.h`,
    the full path must be represented by
    `${LARCOREALG_INC}/larcorealg/Geometry/GeometryCore.h`, with the content
    of `LARCOREALG_INC` variable being an absolute path.
    
    The rest of the loading is performed like with `loadHeader()`.
    """
    # make sure that if there is a INC variable for the package, that one is included
    return self.loadHeader(
      headerRelPath,
      extraPaths
        =([ '$' + self.packageVarNameFromHeaderPath('INC', headerRelPath) ] + extraPaths),
      force=force
      )
  # loadHeaderFromUPS()

  def load(self,
   relPath: "path of the object to be loaded",
   extraPaths: "extra search paths" = [],
   force = False
   ):
    """
    Loads the specified object via ROOT.
    
    This function dispatches the loading call according to whether `relPath`
    appears to be a library (`loadLibrary()`) or not (`loadHeaderFromUPS()`).
    
    All arguments are passed through.
    """
    return (self.loadLibrary if self.isLibrary(relPath) else self.loadHeaderFromUPS)(relPath, extraPaths=extraPaths, force=force)
  # load()
  
  def isLibrary(self, path: "path to be tested") -> "whether `path` is a library":
    """Checks if the `path` appears to be the name of a library."""
    return os.path.splitext(path)[-1] in [ self.PlatformInfo['LibSuffix'], '' ]
  
  def expandLibraryName(self,
   name: "name of the library (no path!)",
   ) -> "library file name":
    """Returns the full library name (`libname.so`) out of the `name`."""
    if not name.startswith('lib'): name = 'lib' + name
    LibSuffix = self.PlatformInfo['LibSuffix']
    if not name.endswith(LibSuffix): name += LibSuffix
    return name
  # expandLibraryName()
  
  @staticmethod
  def packageNameFromHeaderPath(headerPath):
    """Returns the UPS product name out of a "standard" C++ header path."""
    return os.path.split(os.path.dirname(headerPath))[0]
  
  @staticmethod
  def packageVarNameFromHeaderPath(varSuffix, headerPath):
    return SourceCentral.packageNameFromHeaderPath(headerPath).upper() + '_' + varSuffix
  
  @staticmethod
  def LibraryPaths():
    return os.getenv(SourceCentral.PlatformInfo['LibEnvPath']) \
     .split(SourceCentral.PlatformInfo.get('LibEnvPathSep', ':'))
  # LibraryPaths()
  
# class SourceCentral

################################################################################

# global instance of source tracking class
SourceCode = SourceCentral()

################################################################################

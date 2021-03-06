/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.
 For details about the syntax and how to create or use a module, see the
 JUCE Module Format.txt file.


 BEGIN_JUCE_MODULE_DECLARATION

  ID:               rs_boost
  vendor:           RS-MET
  version:          0.0.1
  name:             Rob's Boost Module
  description:      Subset of the boost libraries wrapped into a JUCE module
  website:          https://www.boost.org/
  license:          Boost

  dependencies: 
  OSXFrameworks:
  iOSFrameworks:

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/

#pragma once

#define BOOST_ALL_NO_LIB
// Without this definition, the inclusion of <boost/config/auto_link.hpp>, which happens somewhere 
// deep down in boost between macro definitions and undefinitions (for example in
// <boost/python/detail/config.hpp>), will cause the linker to try to link to a static library file
// that is nowhere specified in the project settings (i really wonder, how that works). So we need
// this definition, to avoid linker errors of the type:
//   cannot open file 'boost_python34-vc142-mt-gd-x64-1_70.lib'
// what all these mt-gd-$&%�@ decorations mean, is explained here:
//   https://www.boost.org/doc/libs/1_60_0/more/getting_started/windows.html#library-naming

#define BOOST_PYTHON_SOURCE     // makes BOOST_PYTHON_DECL expand to __declspec(dllexport)
#define BOOST_NUMPY_SOURCE      // similar for numpy
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION  // gets rid of a warning

//#define BOOST_PYTHON_STATIC_LIB 
//#define BOOST_NUMPY_STATIC_LIB
// may be used instead of .._SOURCE for building a static library? 

#include <boost/python.hpp>
//#include <boost/python/detail/wrap_python.hpp> // alternative - recommended by documentation
#include <boost/python/numpy.hpp>

void initNumPy();
// This should be called in the definition of BOOST_PYTHON_MODULE before using any numpy 
// functionality. It sets up the pointers to the array- and ufunc APIs. Actually, this is 
// supposed to be the job of numpy::initialize(), but for some reason, it doesn't seem to work, 
// so i wrote my own replacement function to get the job done. My version works for me but the 
// boost function doesn't - this is really weird! ...but the ufunc stuff has not yet been tested. 
// Only the array-api stuff is known to work.

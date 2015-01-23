# Test harness SCons build file
#
# D. Robins 20141012

import os, os.path, sys, re


GCC_DIR = 'C:\\MinGW\\bin'


# Output objects in build directory (we don't want common or library code to be built next to its source,
# both for tidiness reasons and because other projects may want to build with different defines/config includes).
def ObjFromSrc(env, rgsSrc):
	rgsObj = []
	for sSrc in rgsSrc:
		sObj = os.path.join('build', os.path.splitext(os.path.basename(sSrc))[0])
		env.Object(sObj, sSrc)
		rgsObj.append(sObj + '$OBJSUFFIX')
	return rgsObj


env = Environment(ENV={'PATH': [GCC_DIR]}, tools=['mingw'])

env.Replace(CPPPATH=[os.path.join(os.environ['GTEST_DIR'], 'include')])
# need gnu++11 instead of c++11 for gtest; can't deal with strict mode
env.Replace(CCFLAGS=['-g'])
env.Replace(CXXFLAGS=['-std=gnu++11' ,'-fno-rtti', '-fno-exceptions'])

envGtest = env.Clone()
envGtest.Append(CPPPATH=[os.environ['GTEST_DIR']])

env.Replace(CPPDEFINES=['UNITTEST'])

# library
rgsObjGtest = ObjFromSrc(envGtest, [os.path.join(os.environ['GTEST_DIR'], 'src', 'gtest-all.cc')])
envGtest.Library('build/gtest', rgsObjGtest)

# files to test
rgsSrc = ['Alloc.cpp']

# sources
rgsSrc += ['test.cpp']

rgsObj = ObjFromSrc(env, rgsSrc)

# build targets
target = 'test'
sBuild = 'build/' + target
env.Program(target=sBuild, source=rgsObj, LIBS=['gtest'], LIBPATH='build')
env.Default(sBuild + '$PROGSUFFIX')


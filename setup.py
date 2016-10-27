from distutils.core import setup, Extension

extension_mod = Extension(name="pydagmodel", sources=["pydagmodel.c", "model.c"], include_dirs=["/home/huynh/parallel2/sys/inst/g/include"], library_dirs=["/home/huynh/parallel2/sys/inst/g/lib"], libraries=["dr"], runtime_library_dirs=["/home/huynh/parallel2/sys/inst/g/lib"])

setup(name="pydagmodel", ext_modules=[extension_mod])

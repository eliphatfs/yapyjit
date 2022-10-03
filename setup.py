import setuptools
from setuptools.command.build_ext import build_ext
import glob
import platform


with open("README.md", "r", encoding='utf-8') as fh:
    long_description = fh.read()


cc_args = []
cl_args = []
if platform.system() == 'Windows':
    cc_args += ['/DEBUG', '/Z7', '/std:c++17']
    cl_args += ['/DEBUG']
else:
    cc_args += ['-D__FUNCTION__=""', '-std=c++17']
    # TODO: change all occurrences


class build_ext_subclass(build_ext):
    def build_extensions(self):
        original_compile = self.compiler._compile
        def new_compile(obj, src, ext, cc_args, extra_postargs, pp_opts):
            if src.endswith('.c'):
                extra_postargs = [s for s in extra_postargs if "c++17" not in s]
            return original_compile(obj, src, ext, cc_args, extra_postargs, pp_opts)
        self.compiler._compile = new_compile
        try:
            build_ext.build_extensions(self)
        finally:
            del self.compiler._compile

setuptools.setup(
    name="yapyjit",
    version="0.0.1a1",
    author="flandre.info",
    author_email="flandre@scarletx.cn",
    description="Yet Another Python JIT",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com",
    include_package_data=True,
    cmdclass={"build_ext": build_ext_subclass},
    ext_modules=[setuptools.Extension(
        "yapyjit",
        glob.glob("src/**/*.c", recursive=True)
        + glob.glob("src/**/*.cpp", recursive=True)
        + ["mir/mir.c", "mir/mir-gen.c"],
        define_macros=[("NO_COMBINE", "1"), ("MIR_INTERP_TRACE", "1")],
        extra_compile_args=cc_args,
        extra_link_args=cl_args
    )],
    packages=setuptools.find_packages(include=['yapyjit_tools', 'yapyjit_tools.*']),
    include_dirs=["include", "mir"],
    classifiers=[
        "Programming Language :: Python :: 3 :: Only",
        "License :: OSI Approved :: Apache Software License"
    ],
    python_requires='~=3.8'
)

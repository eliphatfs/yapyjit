import setuptools
import glob
import platform


with open("README.md", "r") as fh:
    long_description = fh.read()


cc_args = []
cl_args = []
if platform.system() == 'Windows':
    cc_args += ['/DEBUG', '/Z7', '/std:c++17']
    cl_args += ['/DEBUG']
else:
    cc_args += ['-D__FUNCTION__=""', '-std=c++17']
    # TODO: change all occurrences


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
    ext_modules=[setuptools.Extension(
        "yapyjit",
        glob.glob("src/**/*.c", recursive=True)
        + glob.glob("src/**/*.cpp", recursive=True)
        + ["mir/mir.c", "mir/mir-gen.c"],
        define_macros=[("NO_COMBINE", "1"), ("MIR_INTERP_TRACE", "1")],
        extra_compile_args=cc_args,
        extra_link_args=cl_args
    )],
    include_dirs=["include", "mir"],
    classifiers=[
        "Programming Language :: Python :: 3 :: Only",
        "License :: OSI Approved :: Apache Software License"
    ],
    python_requires='~=3.8'
)

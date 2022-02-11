import setuptools
import glob


with open("README.md", "r") as fh:
    long_description = fh.read()


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
        extra_compile_args=['/DEBUG', '/Z7', '/DMIR_INTERP_TRACE=1', '/DNO_COMBINE=1'],
        extra_link_args=['/DEBUG']
    )],
    include_dirs=["include", "mir"],
    classifiers=[
        "Programming Language :: Python :: 3 :: Only",
        "License :: OSI Approved :: Apache Software License"
    ],
    python_requires='~=3.7'
)

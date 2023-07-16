import subprocess

from build.makeproject import MakeProject

class OpenSSLProject(MakeProject):
    def __init__(self, url, alternative_url, md5, installed,
                 **kwargs):
        MakeProject.__init__(self, url, alternative_url, md5, installed, install_target='install_dev', **kwargs)

    def get_make_args(self, toolchain):
        return MakeProject.get_make_args(self, toolchain) + [
            'CC=' + toolchain.cc,
            'CFLAGS=' + toolchain.cflags,
            'CPPFLAGS=' + toolchain.cppflags,
            'AR=' + toolchain.ar,
            'RANLIB=' + toolchain.ranlib,
            'build_libs',
        ]

    def get_make_install_args(self, toolchain):
        # OpenSSL's Makefile runs "ranlib" during installation
        return MakeProject.get_make_install_args(self, toolchain) + [
            'RANLIB=' + toolchain.ranlib,
        ]

    def _build(self, toolchain):
        src = self.unpack(toolchain, out_of_tree=False)

        # OpenSSL has a weird target architecture scheme with lots of
        # hard-coded architectures; this table translates between our
        # host triplet and the OpenSSL target
        openssl_archs = {
            # not using "android-*" because those OpenSSL targets want
            # to know where the SDK is, but our own build scripts
            # prepared everything already to look like a regular Linux
            # build
            'armv7a-linux-androideabi': 'linux-generic32',
            'aarch64-linux-android': 'linux-aarch64',
            'i686-linux-android': 'linux-x86-clang',
            'x86_64-linux-android': 'linux-x86_64-clang',

            # generic Linux (not used by XCSoar)
            'arm-linux-gnueabihf': 'linux-generic32',

            # Kobo
            'armv7a-a8neon-linux-musleabihf': 'linux-generic32',

            # Windows
            'i686-w64-mingw32': 'mingw',
            'x86_64-w64-mingw32': 'mingw64',

            # Apple
            'x86_64-apple-darwin': 'darwin64-x86_64-cc',
            'aarch64-apple-darwin': 'darwin64-arm64-cc',
        }

        openssl_arch = openssl_archs[toolchain.host_triplet]
        cross_compile_prefix = toolchain.host_triplet + '-'

        subprocess.check_call(['./Configure',
                               'no-shared',
                               'no-module', 'no-engine', 'no-static-engine',
                               'no-async',
                               'no-tests',
                               'no-makedepend',
                               'no-asm', # "asm" causes build failures on Windows
                               openssl_arch,
                               '--cross-compile-prefix=' + cross_compile_prefix,
                               '--libdir=lib', # no "lib64" on amd64, please
                               '--prefix=' + toolchain.install_prefix],
                              cwd=src, env=toolchain.env)
        self.build_make(toolchain, src)

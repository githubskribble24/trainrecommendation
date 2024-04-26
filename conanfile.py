from conans import ConanFile, CMake

class ConanPackage(ConanFile):
    name = 'network-monitor'
    version = "0.1.0"

    generators = 'cmake_find_package'

    requires = [
        ('boost/1.74.0'),
        ('libcurl/7.86.0'),
        ('openssl/3.2.1'),
        ('nlohmann_json/3.9.1')
    ]

    default_options = (
        'boost:shared=False',   # shared=False measn: download the Boost static libraries instead of the shared ones, this means the library is included
                                #  and does not have to be found at run-time, but this does mean the binary filesize will be bigger
        'openssl:shared=False',
        'libcurl:shared=False'
    )

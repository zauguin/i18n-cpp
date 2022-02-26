from conans import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout

class I18Npp(ConanFile):
    name = "i18n++"
    version = "0.0"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False], "plugin": [True, False], "tools": [True, False]}
    default_options = {"shared": False, "fPIC": True, "tools": False, "plugin": False}
    requires = "fmt/8.1.1", "boost/1.78.0"

    exports_sources = "CMakeLists.txt", "clang/*", "common/*", "external/*", "include/*", "merge/*", "tests/*"
    generators = "CMakeDeps"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def generate(self):
        toolchain = CMakeToolchain(self, "Ninja")
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.includedirs = ["include"]

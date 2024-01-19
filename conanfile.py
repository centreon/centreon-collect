from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps
import re

class CollectRecipe(ConanFile):
    settings = "os", "arch", "compiler", "build_type"

    def requirements(self):
        if self.distribution() != "almalinux 8":
            self.requires("abseil/20230802.1")
        self.requires("boost/1.83.0")
        self.requires("fmt/9.1.0", override=True)
        self.requires("gtest/1.14.0")
        self.requires("nlohmann_json/3.11.2")
        self.requires("rapidyaml/0.5.0")
        self.requires("spdlog/1.11.0")
        self.requires("zlib/1.3")

    def configure(self):
        self.options['boost'].without_atomic = False
        self.options['boost'].without_chrono = False
        self.options['boost'].without_container = False
        self.options['boost'].without_context = True
        self.options['boost'].without_contract = True
        self.options['boost'].without_coroutine = True
        self.options['boost'].without_date_time = False
        self.options['boost'].without_exception = False
        self.options['boost'].without_fiber = True
        self.options['boost'].without_filesystem = True
        self.options['boost'].without_graph = True
        self.options['boost'].without_graph_parallel = True
        self.options['boost'].without_iostreams = True
        self.options['boost'].without_json = True
        self.options['boost'].without_locale = False
        self.options['boost'].without_log = True
        self.options['boost'].without_math = True
        self.options['boost'].without_mpi = True
        self.options['boost'].without_nowide = True
        self.options['boost'].without_program_options = True
        self.options['boost'].without_python = True
        self.options['boost'].without_random = True
        self.options['boost'].without_regex = True
        self.options['boost'].without_serialization = True
        self.options['boost'].without_stacktrace = True
        self.options['boost'].without_system = False
        self.options['boost'].without_test = True
        self.options['boost'].without_thread = False
        self.options['boost'].without_timer = True
        self.options['boost'].without_type_erasure = True
        self.options['boost'].without_url = True
        self.options['boost'].without_wave = True
        self.options['libssh2'].shared = False

    @staticmethod
    def distribution():
        with open("/etc/os-release") as f:
            lines = f.readlines()
        id_like_re = re.compile(r"ID_LIKE=\"(.*)\"")
        id_re = re.compile(r"^ID=\"(.*)\"")
        version_id_re = re.compile(r"VERSION_ID=\"([0-9]*).*$")
        like = ""
        distrib = ""
        os_version = ""
        for l in lines:
            if like == "":
                m = id_like_re.match(l)
                if m:
                    like = m.group(1)
            if distrib == "":
                m = id_re.match(l)
                if m:
                    distrib = m.group(1)
            if os_version == "":
                m = version_id_re.match(l)
                if m:
                    os_version = m.group(1)
        return f"{distrib} {os_version}"

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def generate(self):
        cmake = CMakeDeps(self)
        cmake.generate()
        tc = CMakeToolchain(self)
        #tc.variable["MYVAR"] = "VALUE"
        #tc.preprocessor_definitions["MYDEFINE"] = "VALUE"
        tc.generate()

project "tests_hash"
    tags { "test" }
    includedirs { "..", "../src" }

    files {
        "../src/hash.h",
        "../src/hash.c",
        "common.h",
        "common.c",
        "hash.c",
    }

    links { "tidy" }

project "tests_html"
    tags { "test" }
    includedirs { "..", "../src" }

    files {
        "../src/html.h",
        "../src/html.c",
        "../src/utils.h",
        "../src/utils.c",
        "common.h",
        "common.c",
        "html.c",
    }

    links { "tidy" }

project "tests_utils"
    tags { "test" }
    includedirs { "..", "../src" }

    files {
        "../src/utils.h",
        "../src/utils.c",
        "common.h",
        "common.c",
        "utils.c",
    }

    links { "curl", "tidy" }

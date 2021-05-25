project "machinatrix"
    files {
        "config.h",
        "dlpo.c",
        "dlpo.h",
        "html.c",
        "html.h",
        "main.c",
        "utils.c",
        "utils.h",
        "wikt.c",
        "wikt.h",
    }

    links { "cjson", "curl", "tidy" }

    configuration "debug"

project "machinatrix_matrix"
    files {
        "config.h",
        "html.c",
        "html.h",
        "matrix.c",
        "utils.c",
        "utils.h",
        "wikt.c",
        "wikt.h",
    }

    links { "cjson", "curl", "tidy" }

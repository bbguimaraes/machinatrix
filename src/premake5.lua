project "machinatrix"
    files {
        "config.h",
        "dlpo.c",
        "dlpo.h",
        "hash.c",
        "hash.h",
        "html.c",
        "html.h",
        "main.c",
        "numeraria_lib.h",
        "numeraria_lib.c",
        "socket.c",
        "socket.h",
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
    }

    links { "cjson", "curl", "tidy" }

project "numeraria"
    files {
        "config.h",
        "numeraria.c",
        "numeraria.h",
        "utils.c",
        "utils.h",
    }

    links { "sqlite3" }

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <getopt.h>
#include <sys/wait.h>
#include <tidybuffio.h>
#include <unistd.h>

#include "config.h"
#include "dlpo.h"
#include "html.h"
#include "utils.h"
#include "wikt.h"

const char *PROG_NAME = NULL, *CMD_NAME = NULL;
#define MAX_ARGS ((size_t)1U)
#define DICT_FILE "/usr/share/dict/words"

typedef bool (*mtrix_cmd_f)(const mtrix_config*, int, const char *const*);
typedef struct {
    const char *name;
    mtrix_cmd_f f;
} mtrix_cmd;

int main(int argc, char *const *argv);
bool parse_args(int *argc, char *const **argv, mtrix_config *config);
void usage(FILE *f);
bool handle_cmd(const mtrix_config *config, int argc, const char *const *argv);
bool handle_file(const mtrix_config *config, FILE *f);
void str_to_args(char *str, size_t max_args, int *argc, char **argv);
bool cmd_help(const mtrix_config *config, int argc, const char *const *argv);
bool cmd_ping(const mtrix_config *config, int argc, const char *const *argv);
bool cmd_word(const mtrix_config *config, int argc, const char *const *argv);
bool cmd_abbr(const mtrix_config *config, int argc, const char *const *argv);
bool cmd_damn(const mtrix_config *config, int argc, const char *const *argv);
bool cmd_parl(const mtrix_config *config, int argc, const char *const *argv);
bool cmd_bard(const mtrix_config *config, int argc, const char *const *argv);
bool cmd_dlpo(const mtrix_config *config, int argc, const char *const *argv);
bool cmd_wikt(const mtrix_config *config, int argc, const char *const *argv);
bool cmd_tr(const mtrix_config *config, int argc, const char *const *argv);

mtrix_cmd COMMANDS[] = {
    {"help", cmd_help},
    {"ping", cmd_ping},
    {"word", cmd_word},
    {"abbr", cmd_abbr},
    {"damn", cmd_damn},
    {"parl", cmd_parl},
    {"bard", cmd_bard},
    {"dlpo", cmd_dlpo},
    {"wikt", cmd_wikt},
    {"tr", cmd_tr},
    {0, 0},
};

int main(int argc, char *const *argv) {
    log_set(stderr);
    PROG_NAME = argv[0];
    mtrix_config config;
    init_config(&config);
    if(!parse_args(&argc, &argv, &config))
        return 1;
    if(config.help) {
        usage(stdout);
        return 0;
    }
    return argc
        ? !handle_cmd(&config, argc, (const char * const*)argv)
        : !handle_file(&config, stdin);
}

bool parse_args(int *argc, char *const **argv, mtrix_config *config) {
    static const char *short_opts = "hvn";
    static const struct option long_opts[] = {
        {"help",    no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"dry-run", no_argument, 0, 'n'},
        {0, 0, 0, 0},
    };
    for(;;) {
        int idx = 0;
        const int c = getopt_long(*argc, *argv, short_opts, long_opts, &idx);
        if(c == -1)
            break;
        switch(c) {
        case 'h': config->help = true; continue;
        case 'v': config->verbose = true; continue;
        case 'n': config->dry = true; continue;
        default: return false;
        }
    }
    *argv += optind;
    *argc -= optind;
    return true;
}

void usage(FILE *f) {
    fprintf(f,
        "usage: %s [options] [<cmd>]\n\n"
        "Options:\n"
        "    -h, --help             this help\n"
        "    -v, --verbose          verbose output\n"
        "    -n, --dry-run          don't access external services\n"
        "Commands:\n"
        "    help:                  this help\n"
        "    ping:                  pong\n"
        "    word:                  random word\n"
        "    damn:                  random curse\n"
        "    abbr <acronym> [<dictionary>]:\n"
        "                           random de-abbreviation\n"
        "    bard:                  random Shakespearean insult\n"
        "    dlpo <term>:           lookup etymology (DLPO)\n"
        "    wikt <term>:           lookup etymology (Wiktionary)\n"
        "    parl:                  use unparliamentary language\n"
        "    tr <term>:             lookup translation (Wiktionary)\n",
        PROG_NAME);
}

bool handle_cmd(const mtrix_config *config, int argc, const char *const *argv) {
    const char *name = *argv++;
    --argc;
    for(mtrix_cmd *cmd = COMMANDS; cmd->name; ++cmd)
        if(!strcmp(name, cmd->name)) {
            CMD_NAME = name;
            return cmd->f(config, argc, argv);
        }
    log_err("unknown command: %s\n", name);
    usage(stderr);
    return false;
}

bool handle_file(const mtrix_config *config, FILE *f) {
    bool ret = true;
    char *buffer = NULL;
    for(;;) {
        size_t len;
        if((len = getline(&buffer, &len, f)) == -1)
            break;
        int argc;
        char *argv[MAX_ARGS + 1];
        str_to_args(buffer, MAX_ARGS + 1, &argc, argv);
        if(!argc)
            continue;
        mtrix_cmd *cmd = COMMANDS;
        while(cmd->name && strcmp(argv[0], cmd->name))
            ++cmd;
        if(!cmd->name) {
            log_err("unknown command: %s\n", argv[0]);
            ret = false;
            break;
        }
        CMD_NAME = cmd->name;
        ret = cmd->f(config, argc - 1, (const char *const*)argv + 1);
        CMD_NAME = 0;
        if(!ret)
            break;
    }
    if(buffer)
        free(buffer);
    return ret;
}

void str_to_args(char *str, size_t max_args, int *argc, char **argv) {
    const char *d = " \n";
    size_t n = 0;
    char *saveptr = NULL, *arg = strtok_r(str, d, &saveptr);
    while(n < max_args && arg) {
        argv[n++] = arg;
        arg = strtok_r(NULL, d, &saveptr);
    }
    *argc = n;
}

bool cmd_help(const mtrix_config *config, int argc, const char *const *argv) {
    if(argc) {
        log_err("wrong number of arguments (%i, want 0)\n", argc);
        return false;
    }
    usage(stdout);
    return true;
}

bool cmd_ping(const mtrix_config *config, int argc, const char *const *argv) {
    if(argc) {
        log_err("wrong number of arguments (%i, want 0)\n", argc);
        return false;
    }
    printf("pong\n");
    return true;
}

bool cmd_word(const mtrix_config *config, int argc, const char *const *_) {
    if(argc) {
        log_err("wrong number of arguments (%i, want 0)\n", argc);
        return false;
    }
    pid_t pid = fork();
    if(pid == -1) {
        log_err("fork: %s\n", strerror(errno));
        return false;
    }
    if(!pid) {
        static const char *argv[] = {"shuf", "-n", "1", DICT_FILE, 0};
        return exec(argv, 0, 0);
    }
    return wait_n(1);
}

bool cmd_abbr(const mtrix_config *config, int argc, const char *const *argv) {
    if(argc != 1) {
        log_err("wrong number of arguments (%i, want 1)\n", argc);
        return false;
    }
    for(const char *c = argv[0]; *c; ++c) {
        int fds[2][2];
        if(pipe(fds[0]) == -1) {
            log_err("pipe: %s\n", strerror(errno));
            return false;
        }
        pid_t pid0 = fork();
        if(pid0 == -1) {
            log_err("fork: %s\n", strerror(errno));
            return false;
        }
        if(!pid0) {
            close(fds[0][0]);
            const char *cargv[] = {"look", 0, 0};
            char arg[] = {*c, '\0'};
            cargv[1] = arg;
            return exec(cargv, 0, fds[0][1]);
        }
        close(fds[0][1]);
        if(pipe(fds[1]) == -1) {
            log_err("pipe: %s\n", strerror(errno));
            return false;
        }
        pid_t pid1 = fork();
        if(!pid1) {
            close(fds[1][0]);
            const char *cargv[] = {"shuf", "-n", "1", 0};
            return exec(cargv, fds[0][0], fds[1][1]);
        }
        close(fds[0][0]);
        close(fds[1][1]);
        FILE *child = fdopen(fds[1][0], "r");
        if(!child) {
            log_err("fdopen: %s\n", strerror(errno));
            return false;
        }
        char *buffer = 0;
        size_t len;
        if((len = getline(&buffer, &len, child)) == -1)
            break;
        if(buffer[len - 1] == '\n')
            buffer[len - 1] = '\0';
        printf("%s%s", c != argv[0] ? " " : "", buffer);
        if(!wait_n(2))
            return false;
    }
    printf("\n");
    return true;
}

bool cmd_damn(const mtrix_config *config, int argc, const char *const *argv) {
    if(argc > 1) {
        log_err("wrong number of arguments (%i, max 1)\n", argc);
        return false;
    }
    int fds[2];
    if(pipe(fds) == -1) {
        log_err("pipe: %s\n", strerror(errno));
        return false;
    }
    pid_t pid = fork();
    if(pid == -1) {
        log_err("fork: %s\n", strerror(errno));
        return false;
    }
    if(!pid) {
        close(fds[0]);
        const char *cargv[] =
            {"shuf", DICT_FILE, "-n", argc ? argv[0] : "3", 0};
        return exec(cargv, 0, fds[1]);
    }
    close(fds[1]);
    FILE *child = fdopen(fds[0], "r");
    if(!child) {
        log_err("fdopen: %s\n", strerror(errno));
        return false;
    }
    printf("You");
    char *buffer = 0;
    for(;;) {
        size_t len;
        if((len = getline(&buffer, &len, child)) == -1)
            break;
        if(buffer[len - 1] == '\n')
            buffer[len - 1] = '\0';
        printf(" %s", buffer);
    }
    printf("!\n");
    if(buffer)
        free(buffer);
    return wait_n(1);
}

bool cmd_parl(const mtrix_config *config, int argc, const char *const *argv) {
    if(argc) {
        log_err("wrong number of arguments (%i, want 0)\n", argc);
        return false;
    }
    static const char *v[] = {
        "A terminological inexactitude [citation needed].\n",
        "Liar.\n-- Australia, 1997\n",
        "Dumbo.\n-- Australia, 1997\n",
        "In Belgium there is no such thing as unparliamentary language.\n"
            "-- Belgium\n",
        "Parliamentary pugilist.\n-- Canada, 1875\n",
        "A bag of wind.\n-- Canada, 1878\n",
        "Inspired by forty-rod whisky.\n-- Canada, 1881\n",
        "Coming into the world by accident.\n-- Canada, 1886\n",
        "Blatherskite.\n-- Canada, 1890\n",
        "The political sewer pipe from Carleton County.\n-- Canada, 1917\n",
        "Lacking in intelligence.\n-- Canada, 1934\n",
        "A dim-witted saboteur.\n-- Canada, 1956\n",
        "Liar.\n-- Canada, 1959\n",
        "Devoid of honour.\n-- Canada, 1960\n",
        "Joker in the house.\n-- Canada, 1960\n",
        "Ignoramus.\n-- Canada, 1961\n",
        "Scurrilous.\n-- Canada, 1961\n",
        "To hell with Parliament attitude.\n-- Canada, 1961\n",
        "Trained seal.\n-- Canada, 1961\n",
        "Evil genius.\n-- Canada, 1962\n",
        "Demagogue.\n-- Canada, 1963\n",
        "Canadian Mussolini.\n-- Canada, 1964\n",
        "Sick animal.\n-- Canada, 1966\n",
        "Pompous ass.\n-- Canada, 1967\n",
        "Crook.\n-- Canada, 1971\n",
        "Does not have a spine.\n-- Canada, 1971\n",
        "Fuddle duddle.\n-- Canada, 1971\n",
        "Pig.\n-- Canada, 1977\n",
        "Jerk.\n-- Canada, 1980\n",
        "Sleazebag.\n-- Canada, 1984\n",
        "Racist.\n-- Canada, 1986\n",
        "Scuzzball.\n-- Canada, 1988\n",
        "Weathervane.\n-- Canada, 2007\n",
        "A piece of shit.\n-- Canada, 2011\n",
        "Like a fart.\n-- Canada, 2016\n",
        "臭罌出臭草 (foul grass grows out of a foul ditch).\n"
            "-- Hong Kong, 1996\n",
        "Bad man.\n-- India, 2012\n",
        "Badmashi.\n-- India, 2012\n",
        "Bag of shit.\n-- India, 2012\n",
        "Bandicoot.\n-- India, 2012\n",
        "Communist.\n-- India, 2012\n",
        "Double-minded.\n-- India, 2012\n",
        "Goonda.\n-- India, 2012\n",
        "Rat.\n-- India, 2012\n",
        "Ringmaster.\n-- India, 2012\n",
        "Scumbag.\n-- India, 2012\n",
        "Benny.\n-- Ireland\n",
        "Pair of bennies.\n-- Ireland\n",
        "Brat.\n-- Ireland\n",
        "Buffoon.\n-- Ireland\n",
        "Chancer.\n-- Ireland\n",
        "Communist.\n-- Ireland\n",
        "Corner boy.\n-- Ireland\n",
        "Coward.\n-- Ireland\n",
        "Fascist.\n-- Ireland\n",
        "Gurrier.\n-- Ireland\n",
        "Guttersnipe.\n-- Ireland\n",
        "Hypocrite.\n-- Ireland\n",
        "Rat.\n-- Ireland\n",
        "Scumbag.\n-- Ireland\n",
        "Scurrilous speaker.\n-- Ireland\n",
        "Yahoo.\n-- Ireland\n",
        "Lying or drunk; has violated the secrets of cabinet, "
            "or doctored an official report.\n-- Ireland\n",
        "Handbagging.\n-- Ireland\n",
        "Fuck you!\n-- Ireland, 2009\n",
        "Gurriers shouting on a street at each other.\n-- Ireland\n",
        "Idle vapourings of a mind diseased.\n-- New Zealand, 1946\n",
        "His brains could revolve inside a peanut shell for a thousand years "
            "without touching the sides.\n-- New Zealand, 1949\n",
        "Energy of a tired snail returning home from a funeral.\n"
            "-- New Zealand, 1963\n",
        "Commo (meaning communist).\n-- New Zealand, 1969\n",
        "Scuttles for his political funk hole.\n-- New Zealand, 1974\n",
        "Highway bandit.\n-- Norway, 2009\n",
        "Bastard.\n-- United Kingdom\n",
        "Blackguard.\n-- United Kingdom\n",
        "Coward.\n-- United Kingdom\n",
        "Deceptive.\n-- United Kingdom\n",
        "Dodgy.\n-- United Kingdom\n",
        "Drunk.\n-- United Kingdom\n",
        "Falsehoods.\n-- United Kingdom\n",
        "Git.\n-- United Kingdom\n",
        "Guttersnipe.\n-- United Kingdom\n",
        "Hooligan.\n-- United Kingdom\n",
        "Hypocrite.\n-- United Kingdom\n",
        "Idiot.\n-- United Kingdom\n",
        "Ignoramus.\n-- United Kingdom\n",
        "Liar.\n-- United Kingdom\n",
        "Pipsqueak.\n-- United Kingdom\n",
        "Rat.\n-- United Kingdom\n",
        "Slimy.\n-- United Kingdom\n",
        "Sod.\n-- United Kingdom\n",
        "Squirt.\n-- United Kingdom\n",
        "Stoolpigeon.\n-- United Kingdom\n",
        "Swine.\n-- United Kingdom\n",
        "Tart.\n-- United Kingdom\n",
        "Traitor.\n-- United Kingdom\n",
        "Wart.\n-- United Kingdom\n",
        "Crooked deals.\n-- United Kingdom\n",
        "Use of banned substances.\n-- United Kingdom\n",
        "Been bought.\n-- United Kingdom\n",
        "Racist.\n-- United Kingdom\n",
        "Has made a career out of lying.\n-- United Kingdom\n",
        "Lying.\n-- Wales\n",
        "Mrs. Windsor.\n-- Wales\n",
        "Economical with the truth.\n",
        "Tired and emotional.\n-- United Kingdom\n",
        "Incapable.\n-- United Kingdom\n",
    };
    static const size_t n = sizeof(v) / sizeof(*v);
    srand(time(NULL));
    printf("%s", v[rand() % n]);
    return true;
}

bool cmd_bard(const mtrix_config *config, int argc, const char *const *argv) {
    if(argc) {
        log_err("wrong number of arguments (%i, want 0)\n", argc);
        return false;
    }
    static const char *v[] = {
        "A most notable coward, an infinite and endless liar, an hourly"
            " promise breaker, the owner of no one good quality.\n"
            "-- All’s Well That Ends Well (Act 3, Scene 6)\n",
        "Away, you starvelling, you elf-skin, you dried neat’s-tongue,"
            " bull’s-pizzle, you stock-fish!\n"
            "-- Henry IV Part I (Act 2, Scene 4)\n",
        "Away, you three-inch fool!\n"
            "-- The Taming of the Shrew (Act 3, Scene 3)\n",
        "Come, come, you froward and unable worms!\n"
            "-- The Taming Of The Shrew (Act 5, Scene 2)\n",
        "Go, prick thy face, and over-red thy fear, Thou lily-liver’d boy.\n"
            "-- Macbeth (Act 5, Scene 3)\n",
        "His wit’s as thick as a Tewkesbury mustard.\n"
            "-- Henry IV Part 2 (Act 2, Scene 4)\n",
        "I am pigeon-liver’d and lack gall.\n"
            "-- Hamlet (Act 2, Scene 2)\n",
        "I am sick when I do look on thee\n"
            "-- A Midsummer Night’s Dream (Act 2, Scene 1)\n",
        "I must tell you friendly in your ear, sell when you can, you are not"
            " for all markets.\n"
            "-- As You Like It (Act 3 Scene 5)\n",
        "If thou wilt needs marry, marry a fool; for wise men know well enough"
            " what monsters you make of them.\n"
            "-- Hamlet (Act 3, Scene 1)\n",
        "I’ll beat thee, but I would infect my hands.\n"
            "-- Timon of Athens (Act 4, Scene 3)\n",
        "I scorn you, scurvy companion.\n"
            "-- Henry IV Part II (Act 2, Scene 4)\n",
        "Methink’st thou art a general offence and every man should beat thee."
            "\n"
            "-- All’s Well That Ends Well (Act 2, Scene 3)\n",
        "More of your conversation would infect my brain.\n"
            "-- The Comedy of Erros (Act 2, Scene 1)\n",
        "My wife’s a hobby horse!\n"
            "-- The Winter’s Tale (Act 2, Scene 1)\n",
        "Peace, ye fat guts!\n"
            "-- Henry IV Part 1 (Act 2, Scene 2)\n",
        "Poisonous bunch-backed toad! \n"
            "-- Richard III (Act 1, Scene 3)\n",
        "The rankest compound of villainous smell that ever offended nostril\n"
            "-- The Merry Wives of Windsor (Act 3, Scene 5)\n",
        "The tartness of his face sours ripe grapes.\n"
            "-- The Comedy of Erros (Act 5, Scene 4)\n",
        "There’s no more faith in thee than in a stewed prune.\n"
            "-- Henry IV Part 1 (Act 3, Scene 3)\n",
        "Thine forward voice, now, is to speak well of thine friend; thine"
            " backward voice is to utter foul speeches and to detract.\n"
            "-- The Tempest (Act 2, Scene 2)\n",
        "That trunk of humours, that bolting-hutch of beastliness, that"
            " swollen parcel of dropsies, that huge bombard of sack, that"
            " stuffed cloak-bag of guts, that roasted Manningtree ox with"
            " pudding in his belly, that reverend vice, that grey Iniquity,"
            " that father ruffian, that vanity in years?\n"
            "-- Henry IV Part 1 (Act 2, Scene 4)\n",
        "Thine face is not worth sunburning.\n"
            "-- Henry V (Act 5, Scene 2)\n",
        "This woman’s an easy glove, my lord, she goes off and on at"
            " pleasure.\n"
            "-- All’s Well That Ends Well (Act 5, Scene 3)\n",
        "Thou art a boil, a plague sore\n"
            "-- King Lear (Act 2, Scene 2)\n",
        "Was the Duke a flesh-monger, a fool and a coward?\n"
            "-- Measure For Measure (Act 5, Scene 1)\n",
        "Thou art as fat as butter.\n"
            "-- Henry IV Part 1 (Act 2, Scene 4)\n",
        "Here is the babe, as loathsome as a toad.\n"
            "-- Titus Andronicus (Act 4, Scene 3)\n",
        "Like the toad; ugly and venomous.\n"
            "-- As You Like It (Act 2, Scene 1`)\n",
        "Thou art unfit for any place but hell.\n"
            "-- Richard III (Act 1 Scene 2)\n",
        "Thou cream faced loon\n"
            "-- Macbeth (Act 5, Scene 3)\n",
        "Thou clay-brained guts, thou knotty-pated fool, thou whoreson obscene"
            " greasy tallow-catch!\n"
            "-- Henry IV Part 1 (Act 2, Scene 4 )\n",
        "Thou damned and luxurious mountain goat.\n"
            "-- Henry V (Act 4, Scene 4)\n",
        "Thou elvish-mark’d, abortive, rooting hog!\n"
            "-- Richard III (Act 1, Scene 3 )\n",
        "Thou leathern-jerkin, crystal-button, knot-pated, agatering,"
            " puke-stocking, caddis-garter, smooth-tongue, Spanish pouch!\n"
            "-- Henry IV Part 1 (Act 2, Scene 4)\n",
        "Thou lump of foul deformity\n"
            "-- Richard III (Act 1, Scene 2)\n",
        "That poisonous bunch-back’d toad!\n"
            "-- Richard III (Act 1, Scene 3)\n",
        "Thou sodden-witted lord! Thou hast no more brain than I have in mine"
            " elbows\n"
            "-- Troilus and Cressida (Act 2, Scene 1)\n",
        "Thou subtle, perjur’d, false, disloyal man!\n"
            "-- The Two Gentlemen of Verona (Act 4, Scene 2)\n",
        "Thou whoreson zed , thou unnecessary letter!\n"
            "-- King Lear (Act 2, Scene 2)\n",
        "Thy sin’s not accidental, but a trade.\n"
            "-- Measure For Measure (Act 3, Scene 1)\n",
        "Thy tongue outvenoms all the worms of Nile.\n"
            "-- Cymbeline (Act 3, Scene 4)\n",
        "Would thou wert clean enough to spit upon\n"
            "-- Timon of Athens (Act 4, Scene 3)\n",
        "Would thou wouldst burst!\n"
            "-- Timon of Athens (Act 4, Scene 3)\n",
        "You poor, base, rascally, cheating lack-linen mate! \n"
            "-- Henry IV Part II (Act 2, Scene 4)\n",
        "You are as a candle, the better burnt out.\n"
            "-- Henry IV Part 2 (Act 1, Scene 2)\n",
        "You scullion! You rampallian! You fustilarian! I’ll tickle your"
            " catastrophe!\n"
            "-- Henry IV Part 2 (Act 2, Scene 1)\n",
        "You starvelling, you eel-skin, you dried neat’s-tongue, you"
            " bull’s-pizzle, you stock-fish–O for breath to utter what is like"
            " thee!-you tailor’s-yard, you sheath, you bow-case, you vile"
            " standing tuck!\n"
            "-- Henry IV Part 1 (Act 2, Scene 4)\n",
        "Your brain is as dry as the remainder biscuit after voyage.\n"
            "-- – As You Like It (Act 2, Scene 7)\n",
        "Virginity breeds mites, much like a cheese.\n"
            "-- All’s Well That Ends Well (Act 1, Scene 1)\n",
        "Villain, I have done thy mother\n"
            "-- Titus Andronicus (Act 4, Scene 2)\n",
    };
    static const size_t n = sizeof(v) / sizeof(*v);
    srand(time(NULL));
    printf("%s", v[rand() % n]);
    return true;
}

bool cmd_dlpo(const mtrix_config *config, int argc, const char *const *argv) {
    if(!argc) {
        log_err("missing argument\n");
        return false;
    }
    const char *url_parts[] =
        {"https://dicionario.priberam.org/", *argv, NULL};
    char url[MTRIX_MAX_URL_LEN];
    if(!build_url(url, url_parts))
        return false;
    if(config->verbose)
        printf("Looking up term: %s\n", url);
    if(config->dry)
        return true;
    mtrix_buffer buffer = {NULL, 0};
    if(!request(url, &buffer, config->verbose)) {
        free(buffer.p);
        return false;
    }
    TidyDoc tidy_doc = tidyCreate();
    tidyOptSetBool(tidy_doc, TidyForceOutput, yes);
    tidyParseString(tidy_doc, buffer.p);
    free(buffer.p);
    const char *id = "resultados";
    TidyNode res = find_node_by_id(tidyGetRoot(tidy_doc), id, true);
    bool ret = false;
    if(!res) {
        log_err("element '#%s' not found\n", id);
        goto cleanup;
    }
    TidyNode def = dlpo_find_definitions(tidy_doc, res);
    if(!def)
        goto cleanup;
    dlpo_print_definitions(tidy_doc, def);
    ret = true;
cleanup:
    tidyRelease(tidy_doc);
    return ret;
}

bool cmd_wikt(const mtrix_config *config, int argc, const char *const *argv) {
    if(!argc) {
        log_err("missing argument\n");
        return false;
    }
    const char *url_parts[] = {"https://en.wiktionary.org/wiki/", *argv, NULL};
    char url[MTRIX_MAX_URL_LEN];
    if(!build_url(url, url_parts))
        return false;
    if(config->verbose)
        printf("Looking up term: %s\n", url);
    if(config->dry)
        return true;
    mtrix_buffer buffer = {NULL, 0};
    if(!request(url, &buffer, config->verbose)) {
        free(buffer.p);
        return false;
    }
    TidyDoc tidy_doc = tidyCreate();
    tidyOptSetBool(tidy_doc, TidyForceOutput, yes);
    tidyParseString(tidy_doc, buffer.p);
    free(buffer.p);
    bool ret = false;
    wikt_page page;
    if(!wikt_parse_page(tidy_doc, &page))
        goto cleanup;
    for(TidyNode lang = page.contents; lang;) {
        TidyNode sect = lang;
        TidyAttr lang_id = find_attr(tidyGetChild(lang), "id");
        const char *lang_text = lang_id ? tidyAttrValue(lang_id) : "?";
        while(wikt_next_section("h3", "Etymology", 9, &sect)) {
            if(lang_text) {
                printf("%s\n", lang_text);
                lang_text = NULL;
            }
            TidyBuffer buf = {0};
            tidyNodeGetText(tidy_doc, tidyGetNext(sect), &buf);
            join_lines(buf.bp, buf.bp + buf.size);
            printf("  ");
            print_unescaped(stdout, buf.bp);
            printf("\n");
            tidyBufFree(&buf);
        }
        lang = sect;
    }
    ret = true;
cleanup:
    tidyRelease(tidy_doc);
    return ret;
}

bool cmd_tr(const mtrix_config *config, int argc, const char *const *argv) {
    if(argc < 1) {
        log_err("at least one argumnet (term) is required");
        return false;
    }
    const char *url_parts[] = {"https://en.wiktionary.org/wiki/", *argv, NULL};
    char url[MTRIX_MAX_URL_LEN];
    if(!build_url(url, url_parts))
        return false;
    if(config->verbose)
        printf("Looking up term: %s\n", url);
    if(config->dry)
        return true;
    mtrix_buffer buffer = {NULL, 0};
    if(!request(url, &buffer, config->verbose)) {
        free(buffer.p);
        return false;
    }
    TidyDoc tidy_doc = tidyCreate();
    tidyOptSetBool(tidy_doc, TidyForceOutput, yes);
    tidyParseString(tidy_doc, buffer.p);
    free(buffer.p);
    bool ret = false;
    wikt_page page;
    if(!wikt_parse_page(tidy_doc, &page))
        goto cleanup;
    TidyBuffer buf = {0};
    for(TidyNode lang = page.contents; lang;) {
        TidyNode sect = lang;
        TidyAttr lang_id = find_attr(tidyGetChild(lang), "id");
        const char *lang_text = lang_id ? tidyAttrValue(lang_id) : "?";
        while(wikt_next_subsection("div", "Translations-", 13, &sect)) {
            if(lang_text) {
                printf("%s\n", lang_text);
                lang_text = NULL;
            }
            const TidyNode head = wikt_translation_head(sect);
            if(!head)
                continue;
            tidyNodeGetText(tidy_doc, head, &buf);
            join_lines(buf.bp, buf.bp + buf.size);
            printf("  %s\n", buf.bp);
            tidyBufFree(&buf);
            const TidyNode body = wikt_translation_body(sect);
            if(!body)
                continue;
            for(TidyNode td = tidyGetChild(body); td; td = tidyGetNext(td)) {
                TidyNode n = td;
                if(!(n = tidyGetChild(n)))
                    continue;
                if(!(n = tidyGetChild(n)))
                    continue;
                for(TidyNode li = n; li; li = tidyGetNext(li)) {
                    tidyNodeGetText(tidy_doc, li, &buf);
                    join_lines(buf.bp, buf.bp + buf.size);
                    printf("    ");
                    print_unescaped(stdout, buf.bp);
                    printf("\n");
                    tidyBufFree(&buf);
                }
            }
        }
        lang = sect;
    }
    ret = true;
cleanup:
    tidyRelease(tidy_doc);
    return ret;
}

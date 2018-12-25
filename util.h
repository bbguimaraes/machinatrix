#include <stdbool.h>

void log_err(const char *fmt, ...);
bool copy_arg(const char *name, char *dst, const char *src, size_t max);
bool exec(const char *const *argv, int fin, int fout);
bool wait_n(size_t n);
void join_lines(unsigned char *b, unsigned char *e);

#define MTRIX_MAX_URL 1024
typedef struct { char *p; size_t s; } mtrix_buffer;
size_t mtrix_buffer_append(char *in, size_t size, size_t n, mtrix_buffer *b);

bool build_url(char *url, const char **v);
bool request(const char *url, mtrix_buffer *b, bool verbose);
bool post(
    const char *url, size_t n, const char *data,
    mtrix_buffer *b, bool verbose);

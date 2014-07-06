#include "common.h"

typedef struct {
    uint count;
    uint alloc;
    uint* list;
} fgl; // fourgram list

fgl fourgram[37][37][37][37] = {{{{{.count = 0, .alloc = 0, .list = NULL}}}}};

typedef struct {
    uint count;
    uint alloc;
    void** list;
} addrl; // mem address list

addrl domain_map = {.count=0, .alloc=0, .list=NULL};

#define offset(c) ((c)>='A' && (c)<='Z' ? (c)-'A' : ((c)>='0' && (c)<='9' ? (c)-'0'+26 : 36))
static fgl* get_fgl(char* str){
    return &fourgram[offset(str[0])][offset(str[1])][offset(str[2])][offset(str[3])];
}

static void add_fourgram(char *str, uint idx){
    fgl* f = get_fgl(str);
    if (f->count >= f->alloc){
        f->alloc = (int)(f->count * 1.3 + 8);
        f->list = safe_realloc(f->list, sizeof(uint) * f->alloc);
    }
    f->list[f->count++] = idx;
}

static uint add_domain_map(char* domain){
    if (domain_map.count >= domain_map.alloc){
        domain_map.alloc = (int)(domain_map.count * 1.3) + 1024;
        domain_map.list = safe_realloc(domain_map.list, sizeof(char*) * domain_map.alloc);
    }
    domain_map.list[domain_map.count++] = (void*)domain;
    return domain_map.count - 1;
}

// ^[A-Z0-9]+(\.[A-Z0-9-]+)+$
#define allowedchar(c) (((c)>='A' && (c)<='Z') || ((c)>='0' && (c)<='9') || (c)=='-')
static bool is_valid_domain(char* begin, char* end){
    if (begin >= end)
        return false;
    if (allowedchar(*begin)) {
        ++begin;
        goto nextchar;
    }
    return false;
nextchar:
    if (allowedchar(*begin)) {
        ++begin;
        goto nextchar;
    }
    if (*begin == '.') {
        ++begin;
        goto begintld;
    }
    return false;
begintld:
    if (begin == end) // tld should not be empty
        return false;
    if (allowedchar(*begin)) {
        ++begin;
        goto nextchar2;
    }
    return false;
nextchar2:
    if (begin == end)
        return true;
    if (allowedchar(*begin)) {
        ++begin;
        goto nextchar2;
    }
    if (*begin == '.') {
        ++begin;
        goto begintld;
    }
	return false;
}

static void load_data(char* text, size_t len){
    char *end = text + len;
    uint valid_domain_count = 0;
    uint invalid_domain_count = 0;
    while (text < end) {
        char *domain = text;
        while (*text != '\n') {
            ++text;
        }
        if (!is_valid_domain(domain, text)) {
            ++text; // skip \n
            ++invalid_domain_count;
            continue;
        }
        uint idx = add_domain_map(domain);
        if (text - domain >= 5) {
            while (domain[3] != '.'){
                add_fourgram(domain, idx);
                ++domain;
            }
        }
        ++text; // skip \n
        ++valid_domain_count;
    }
    printf("Loaded %d valid domains, %d invalid lines\n", valid_domain_count, invalid_domain_count);
}

// pattern terminated by \0, domain terminated by \n
static bool check_pattern(char* pattern, char* domain, uint domain_len){
    char* domain_cstr = safe_malloc(domain_len + 1);
    memcpy(domain_cstr, domain, domain_len);
    domain_cstr[domain_len] = '\0';
    bool retval = (0 == fnmatch(pattern, domain_cstr, FNM_NOESCAPE));
    free(domain_cstr);
    return retval;
}

// is wildcard char
#define iswc(c) ((c)=='*'||(c)=='?')

// return a \n separated string of domains
char* search_pattern(char* pattern, uint skip, uint count){
    char* p = pattern;
    char* end = pattern + strlen(pattern);
    if (end - p < 4 || p[0] == '.' || p[1] == '.' || p[2] == '.')
        return NULL; // must have consecutive 4 non-wildcards

    uint min_fourgram_count = 0xFFFFFFFF; // maxint
    char* min_fourgram = NULL;
    while (end - p >= 4){
        if (p[3] == '.')
            break;
        if (!iswc(p[0]) && !iswc(p[1]) && !iswc(p[2]) && !iswc(p[3])){
			int count = get_fgl(p)->count;
            if (count < min_fourgram_count) {
                min_fourgram_count = count;
                min_fourgram = p;
            }
        }
        ++p;
    }
    if (min_fourgram == NULL)
        return NULL;
	debug("search: min_fourgram=%c%c%c%c count=%d\n", min_fourgram[0], min_fourgram[1], min_fourgram[2], min_fourgram[3], min_fourgram_count);

    uint* l = get_fgl(min_fourgram)->list;
    char *result = safe_malloc(1);
    result[0] = '\0';
    uint result_len = 0;
    uint found_count = 0;

    uint i;
    for (i=0; i<min_fourgram_count; i++){
        char *domain = (char*)domain_map.list[l[i]];
        uint length = 0;
        while (domain[length] != '\n')
            ++length;

        if (check_pattern(pattern, domain, length)) {
            ++found_count;
            if (found_count <= skip)
                continue;

            result = safe_realloc(result, result_len + length + 2);
            memcpy(result + result_len + 1, domain, length);
            result[result_len] = '$';
            result_len += length + 1;
            result[result_len] = '\0';
            if (found_count >= skip + count)
                return result;
        }
    }
    return result;
}

int main(int argc, char** argv) {
    int port = 7000;
    int fd;
    struct stat sb;
    char *domain_file;
	char *buf;

    if (argc < 2){
		fprintf(stderr, "usage: %s <domain-file> [<port>]\n", argv[0]);
        return 1;
    }

    IF_ERROR((fd = open(argv[1], O_RDONLY)), "open")
    IF_ERROR(fstat(fd, &sb), "fstat")
    domain_file = mmap(0, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    ASSERT(domain_file != MAP_FAILED)
    IF_ERROR(close(fd), "close");

	printf("Loading %s...\n", argv[1]);
	buf = safe_malloc(sb.st_size + 2);
	memcpy(buf, domain_file, sb.st_size);
	buf[sb.st_size] = '\n'; // in case file is not terminated by \n
	buf[sb.st_size+1] = '\0';

	munmap(domain_file, sb.st_size);

	printf("File loaded into memory. Parsing...\n");
    load_data(buf, sb.st_size);

    if (argc == 3) {
        port = atoi(argv[1]);
    }
    init_server(port);

	return 0; // should never reach here
}

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <getopt.h>
#include <errno.h>
#include <stdarg.h>

#define OPTS       "0achnqrtv"
#define SELF       "count-file"
#define USAGE      "Usage: " SELF " [-" OPTS "] <directory> [dir] [...]"
#define VERSION    "0.9"

#define HELP       "\n  "  USAGE "\n\n\
      -0     Do not print empty directories.\n\
      -a     Count file that begin with dot.\n\
      -c     Produce a grand total.\n\
      -d     TODO: File's depth in the directory tree\n\
      -h     Display this message and exit.\n\
      -L     TODO: Follow symbolic links.\n\
      -m <m> TODO: Do not display directory with less than <m> files.\n\
      -M <M> TODO: Do not display directory with more than <M> files.\n\
      -n     Do not print directory name.\n\
      -P     TODO: Never follow symbolic links. (default)\n\
      -q     Be quiet please.\n\
      -r     Count recursively (but don't show tree without -t).\n\
      -t     Print subdirectory recursively (but don't sum them without -r).\n\
      -v     Display version and exit.\n\n"

/* depends on -n, see "format" */
#define AFORMAT    "%-7d %s\n"
#define QFORMAT    "%d\n"

#define TREE       1
#define SUM        2
#define GRANDTOTAL 4

int grandTotal = 0;

/* format to be passed to _print */
char *format = AFORMAT;

/* pointer to function for debug message */
int (*_print) (FILE * stream, const char *format, ...);

/* do nothing */
int _quiet (FILE * unused, const char *unsed, ...)
{
    return 1;
}

/* display message and quit */
void _quit (int err, char *format, ...)
{
    va_list arglist;

    va_start (arglist, format);
    vfprintf (stderr, format, arglist);
    va_end (arglist);

    exit (err);
}

/* is "path" a directory ? */
int is_dir (char *path)
{
    struct stat st;
    if (lstat (path, &st) == -1)
        return 0;
    return S_ISDIR (st.st_mode);
    /* return ((st.st_mode & S_IFMT) == S_IFLNK); */
}

/* is "tmp->d_name" begin with a dot ? */
int _not_dot (const struct dirent *tmp)
{
    if (tmp->d_name[0] == '.')
        return 0;
    return 1;
}

/* is "tmp->d_name" "." or ".." ? */
int _almost_all (const struct dirent *tmp)
{
    if (tmp->d_name[0] == '.')
        if (tmp->d_name[1] == 0 || (tmp->d_name[1] == '.' && tmp->d_name[2] == 0))
            return 0;
    return 1;
}

/* read through "path", count file and call itself for every directory */
int _count (char *path, int (*_filter) (const struct dirent *), int depth, int threshold,
              int mode)
{
    struct dirent **namelist;
    char *next = NULL;
    int thisNb, thisTotal, n, i, tmp;

    thisNb = thisTotal = n = scandir (path, &namelist, _filter, 0);

    /* scandir failed */
    if (n < 0) {
        _print (stderr, "%s: cannot read directory `%s': %s\n", SELF, path, strerror (errno));
        /* return 0 because of "grandTotal" in main and "thisNb" below */
        return 0;
    }

    /* for each file */
    while ((mode & SUM || mode & TREE) && n-- && n >= 0) {

        /* get length of the whole path and malloc */
        i = strlen (path) + strlen (namelist[n]->d_name) + 2;
        next = (char *) malloc (i * sizeof (char));

        /* store the whole path in "next" */
        tmp = sprintf (next, "%s/%s", path, namelist[n]->d_name);
        next[i - 1] = 0;

        /* if "next" is a directory */
        if (is_dir (next))
            /* recall _count */
            thisTotal += _count (next, _filter, depth + 1, threshold, mode);

        free (namelist[n]);
        free (next);
    }

    free (namelist);

    if (thisNb >= threshold) {

        /* display tree and sum */
        if (((mode & SUM) && (mode & TREE))) {
            printf (format, thisTotal, path);
            return thisTotal;

        /* display sum */
        } else if ((mode & SUM)) {
            if (depth == 0)
                printf (format, thisTotal, path);
            return thisTotal;

        /* display tree */
        } else if (mode & TREE) {
            printf (format, thisNb, path);
            return thisNb;

        /* display number of file in this directory */
        } else if (depth == 0) {
            printf (format, thisNb, path);
            return thisNb;

        } else {
            _quit (1, "BUG, line %d\n", __LINE__);
            return 0;
        }
    }

    return 0;
}


/* main */
int main (int argc, char **argv)
{
    int i, n, opts, grandTotal = 0, threshold = 0, mode = 0;

    int (*_filter) (const struct dirent *);

    /* assign default value */
    _filter = _not_dot;
    _print = fprintf;

    /* tune getopt */
    opterr = 0;
    optarg = 0;

    /* get options */
    while ((opts = getopt (argc, argv, "+" OPTS)) != -1) {

        /* don't display empty directory */
        if (opts == '0')
            threshold = 1;

        /* all file (except "." and "..") */
        else if (opts == 'a')
            _filter = _almost_all;

        /* display sum of all */
        else if (opts == 'c')
            mode |= GRANDTOTAL;

        /* display sum of all */
        else if (opts == 'h')
            _quit (0, "%s", HELP);

        /* display number only */
        else if (opts == 'n')
            format = QFORMAT;

        /* be quiet */
        else if (opts == 'q')
            _print = _quiet;

        /* recurse */
        else if (opts == 'r')
            mode |= SUM;

        /* display files recursively */
        else if (opts == 't')
            mode |= TREE;

        /* display version and die */
        else if (opts == 'v')
            _quit (0, "%s: version %s\n", SELF, VERSION);

        /* lame user. die. */
        else
            _quit (0, "  %s\n", USAGE);
    }

    /* number of argument */
    n = argc - optind;

    /* if no argument, count file in current dir */
    if (n == 0)
        grandTotal = _count (".", _filter, 0, threshold, mode);

    else
        for (i = optind; i < argc; i++)
            grandTotal += _count (argv[i], _filter, 0, threshold, mode);

    if (mode & GRANDTOTAL)
        printf (format, grandTotal, "total");

    return (0);
}

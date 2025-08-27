/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Lucas Holt
 * Copyright (c) 2008 Chris Reinhardt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <mport.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>
#include <ftw.h>

#ifdef PRINT_DIAG
#define DIAG(fmt, ...) warnx(fmt, ##__VA_ARGS__);
#else
#define DIAG(fmt, ...) 
#endif

static void usage(void);
static int check_fake(mportAssetList *, const char *, const char *, const char *);
static int grep_file(const char *, const char *);
static bool is_in_plist(const char *relative_path, const char *prefix);
static int check_missing_from_plist(const char *path, const struct stat *st, int typeflag, struct FTW *ftwbuf);
static void check_for_missing_files(const char *destdir, const char *prefix, mportAssetList *assetlist);
 


// Global variables to hold the destdir and assetlist for comparison
static const char *global_destdir;
static const char *global_prefix;
static mportAssetList *global_assetlist;

int
main(int argc, char *argv[]) 
{
	int ch, ret;
	const char *skip = NULL, *prefix = NULL, *destdir = NULL, *assetlistfile = NULL;
	mportAssetList *assetlist;
	FILE *fp;
	const char *chroot_path = NULL;

	while ((ch = getopt(argc, argv, "c:f:d:s:p:")) != -1) {
		switch (ch) {
			case 'c':
				chroot_path = optarg;
				break;
			case 's':
				skip = optarg;
				break;
			case 'p':
				prefix = optarg;
				break;
			case 'd':
				destdir = optarg;
				break;
			case 'f':
				assetlistfile = optarg;
				break;
			case '?':
			default:
				usage();
				break; 
		}
	} 

	argc -= optind;
	argv += optind;

	if (!prefix || !destdir || !assetlistfile) 
		usage();

	DIAG("assetlist = %s; destdir = %s; prefix = %s; skip = %s", assetlistfile, destdir, prefix, skip)

	if (chroot_path != NULL) {
		if (chroot(chroot_path) == -1) {
			err(EXIT_FAILURE, "chroot failed");
		}
	}
	
	if ((fp = fopen(assetlistfile, "r")) == NULL)
		err(EX_NOINPUT, "Could not open assetlist file %s", assetlistfile);
			
	if ((assetlist = mport_assetlist_new()) == NULL) 
		err(EX_OSERR, "Could not not allocate assetlist");
	
	if (mport_parse_plistfile(fp, assetlist) != MPORT_OK)
		err(EX_DATAERR, "Invalid assetlist");

	DIAG("running check_fake")
	
	printf("Checking %s\n", destdir);
	ret = check_fake(assetlist, destdir, prefix, skip);
	check_for_missing_files(destdir, prefix, assetlist);
	
	if (ret == 0) {
		printf("Fake succeeded.\n");
	} else {
		printf("Fake failed.\n");
	}
	
	mport_assetlist_free(assetlist);
	
	return ret;
}

static int
check_fake(mportAssetList *assetlist, const char *destdir, const char *prefix, const char *skip)
{
	mportAssetListEntry *e;
	char cwd[FILENAME_MAX], file[FILENAME_MAX];
	char *anchored_skip;
	struct stat st;
	regex_t skipre;
	int ret = 0;

	DIAG("checking skip: %s", skip)
		
	if (skip != NULL) {
		DIAG("Compiling skip: %s", skip)
		
		if (asprintf(&anchored_skip, "^%s$", skip) == -1)
			err(EX_OSERR, "Could not build skip regex");
		
		if (regcomp(&skipre, anchored_skip, REG_EXTENDED|REG_NOSUB) != 0)
			errx(EX_DATAERR, "Could not compile skip regex");
			
		free(anchored_skip);	
	}
	
	DIAG("Coping prefix (%s) to cwd", prefix)
	
	(void)strlcpy(cwd, prefix, FILENAME_MAX);

	DIAG("Starting loop, cwd: %s", cwd)
	
	STAILQ_FOREACH(e, assetlist, next) {
		if (e->type == ASSET_CWD) {
				if (e->data == NULL) {
					DIAG("Setting cwd to '%s'", prefix)
					(void)strlcpy(cwd, prefix, FILENAME_MAX);
				} else {
					DIAG("Setting cwd to '%s'", e->data)
					(void)strlcpy(cwd, e->data, FILENAME_MAX);
				}
				
				break;
		}


		if (e->type == ASSET_DIR) {
			DIR *dir = opendir(file);
			if (dir != NULL) {
				struct dirent *entry;
				int is_empty = 1;
				while ((entry = readdir(dir)) != NULL) {
					if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
						is_empty = 0;
						break;
					}
				}
				closedir(dir);
				if (is_empty) {
					printf("    %s is an empty directory\n", file);
					// we might not want to fail here, but we should warn user.
					// ret = 1;
				}
			}

			break;
		}

		if (e->data != NULL && (strcmp(e->data, "+CONTENTS") == 0 || strcmp(e->data, "+DESC") == 0)) {
			if (lstat(file, &st) != 0) {
				printf("    Metadata file %s is missing\n", file);
				ret = 1;
			}
			continue;
		}
	
		// skip sample files until we can fix the edge cases with alternate file names. e->type != ASSET_SAMPLE	
		if (e->type != ASSET_FILE && e->type != ASSET_FILE_OWNER_MODE && e->type != ASSET_INFO && e->type != ASSET_SAMPLE_OWNER_MODE && e->type != ASSET_SHELL)
			continue;

		if (e->data[0] == '/')
			(void)snprintf(file, FILENAME_MAX, "%s%s", destdir, e->data);
		else		
			(void)snprintf(file, FILENAME_MAX, "%s%s/%s", destdir, cwd, e->data);

		DIAG("checking %s", file)
			
		if (lstat(file, &st) != 0) {
			(void)snprintf(file, FILENAME_MAX, "%s/%s", cwd, e->data);
			
			if (lstat(file, &st) == 0) {
				(void)printf("		%s installed in %s\n", e->data, cwd);
			} else {
				(void)printf("		%s not installed.\n", e->data);
			}
			
			ret = 1;
			continue;
		}
		
		// symlink checks.
		if (S_ISLNK(st.st_mode)) {
			char target[FILENAME_MAX];
			ssize_t len = readlink(file, target, sizeof(target) - 1);
			if (len == -1) {
				(void)printf("    %s is a broken symlink\n", file);
				ret = 1;
			} else {
				target[len] = '\0';
				// Resolve the symlink target relative to the directory containing
				// the link
				char resolved_target[FILENAME_MAX];
				if (realpath(file, resolved_target) == NULL) {
					(void)printf("    WARN: %s points to an invalid target: %s\n",
					    file, target);
					//ret = 1;
				} else if (strncmp(resolved_target, destdir, strlen(destdir)) !=
				    0) {
					(void)printf("    WARN: %s points outside the destdir: %s\n",
					    file, resolved_target);
					//ret = 1;
				}
			}
			continue;
		}

		/* if file matches skip continue */
		if (skip != NULL && (regexec(&skipre, e->data, 0, NULL, 0) == 0))
			continue;

		if (S_ISREG(st.st_mode) && st.st_size == 0) {
			// we don't fail, just warn for an empty file.
			(void)printf("    %s is an empty file\n", file);
			continue;
		}

	
		if (e->data != NULL) {
			const char *ext = strrchr(e->data, '.'); // Find the last occurrence of '.'
			if (ext != NULL && 
				(strcmp(ext, ".tmp") == 0 || strcmp(ext, ".bak") == 0 || strcmp(ext, ".debug") == 0)) {
				printf("    %s is a temporary or debug file and should not be installed\n", file);
				ret = 1;
				continue;
			}
		}

		DIAG("==> Grepping %s", file)
		/* grep file for fake destdir */
		if (grep_file(file, destdir)) {
			(void)printf("		%s contains the fake destdir\n", e->data);
			ret = 1;
			continue;
		}

		if (S_ISREG(st.st_mode) && access(file, X_OK) == 0) {

			if (!mport_is_elf_file(file) ) {
				DIAG("Skipping ldd check for non-elf file: %s", file);
				continue;
			}

			if (mport_is_statically_linked(file)) {
				DIAG("Skipping ldd check for statically linked file: %s", file);
				continue;
			}

			// Check if the file is a shell script
			if (strstr(e->data, ".sh") != NULL) {
				DIAG("Skipping ldd check for shell script: %s", file);
				continue;
			} else if (strstr(e->data, ".pl") != NULL) {
				DIAG("Skipping ldd check for Perl script: %s", file);
				continue;
			} else if (strstr(e->data, ".py") != NULL) {
				DIAG("Skipping ldd check for Python script: %s", file);
				continue;
			} else if (strstr(e->data, ".rb") != NULL) {
				DIAG("Skipping ldd check for Ruby script: %s", file);
				continue;
			} else if (strstr(e->data, ".js") != NULL) {
				DIAG("Skipping ldd check for JavaScript script: %s", file);
				continue;
            } else if (strstr(e->data, ".lua") != NULL) {
				DIAG("Skipping ldd check for Lua script: %s", file);
				continue;
			} else if (strstr(e->data, ".php") != NULL) {
				DIAG("Skipping ldd check for PHP script: %s", file);
				continue;
			} else if (strstr(e->data, ".awk") != NULL) {
				DIAG("Skipping ldd check for AWK script: %s", file);
				continue;
			}

			// check if .la file
			if (strstr(e->data, ".la") != NULL) {
				DIAG("Skipping ldd check for libtool archive: %s", file);
				continue;
			}

			FILE *f = fopen(file, "r");
			if (f != NULL) {
				char shebang[3] = { 0 }; // Buffer to hold the first two characters
				if (fread(shebang, 1, 2, f) == 2 && strcmp(shebang, "#!") == 0) {
					DIAG("Skipping ldd check for shell script: %s", file);
					fclose(f);
					continue;
				}
				fclose(f);
			}

			char cmd[FILENAME_MAX];
			(void)snprintf(cmd, sizeof(cmd), "ldd %s > /dev/null 2>&1", file);
			if (system(cmd) != 0) {
				(void)printf("    %s has missing or broken dependencies\n", file);
				ret = 1;
			}
		}
	}
	
	if (skip != NULL)
		regfree(&skipre);
	
	return ret;
} 
			

static int
grep_file(const char *filename, const char *destdir)
{
	FILE *file;
	char *line, *nline;
	char *destdir_fixed;
	static regex_t regex;
	static int compiled = 0;
	size_t len;
	int ret = 0;
	
	DIAG("===> Compiling destdir: %s", destdir)
	
	/* Should we cache the compiled regex? */
	if (!compiled) {
		/* + is a special character, deal with it so archivers/zipios++ works */
		destdir_fixed = mport_string_replace(destdir, "+", "\\+");
		DIAG("===> destdir_fixed for regular expression: %s", destdir_fixed)
		if (regcomp(&regex, destdir_fixed, REG_EXTENDED|REG_NOSUB) != 0)
			errx(EX_DATAERR, "Could not compile destdir regex");
		free(destdir_fixed);
		compiled++;
	}
	
	if ((file = fopen(filename, "r")) == NULL)
		err(EX_SOFTWARE, "Couldn't open %s", filename);
		
	while ((line = fgetln(file, &len)) != NULL) {
		nline = NULL;
		/* if we end in \n just switch it to \0, otherwise we need more mem */
		if (line[len - 1] == '\n') {
			line[len - 1] = '\0';
		} else {
			nline = (char *)malloc((len + 1) * sizeof(char));
			if (nline == NULL)
				err(EX_OSERR, "Couldn't allocate nline buf");
			
			memcpy(nline, line, len);
			nline[len] = '\0';
			line = nline;
		}
		
		if (regexec(&regex, line, 0, NULL, 0) == 0) {
			DIAG("===> Match line: %s", line);
			free(nline);
			ret = 1;
			break;
		}
		free(nline);
	}
	
	if (ferror(file) != 0)
		err(EX_IOERR, "Error reading %s", filename);
	
	regfree(&regex);
	fclose(file);
	return ret;
}


static bool 
is_in_plist(const char *relative_path, const char *prefix) {
    mportAssetListEntry *e;

    STAILQ_FOREACH (e, global_assetlist, next) {
		if (e->data == NULL) {
			continue; // Skip entries with NULL data
		}

	    // Check for an exact match (absolute path)
	    if (strcmp(e->data, relative_path) == 0) {
		    return true;
	    }

	    // Check for a match relative to the prefix
	    char prefixed_path[FILENAME_MAX];
	    snprintf(prefixed_path, sizeof(prefixed_path), "%s/%s", prefix, e->data);
	    if (strcmp(prefixed_path, relative_path) == 0) {
		    return true;
	    }
    }

    return false;
}

static int 
check_missing_from_plist(const char *path, const struct stat *st __attribute__((unused)), int typeflag, struct FTW *ftwbuf __attribute__((unused))) 
{
	if (path == NULL) {
		(void)fprintf(stderr, "Error: nftw provided a NULL path\n");
		return 0;
	}

    // Skip directories; only check files
    if (typeflag == FTW_D || typeflag == FTW_DP) {
        return 0;
    }

	if (strncmp(path, global_destdir, strlen(global_destdir)) != 0) {
		(void)fprintf(stderr, "Error: Path '%s' does not start with destdir '%s'\n", path, global_destdir);
		return 0;
	}

    // Get the relative path of the file
    const char *relative_path = path + strlen(global_destdir);

	// Skip `dir` files in `/usr/local/share/info`, generated by indexinfo(1)
	if (strcmp(relative_path, "/usr/local/share/info/dir") == 0) {
        return 0; 
    }

    // Check if the file is in the plist
    if (!is_in_plist(relative_path, global_prefix)) {
        (void)printf("    %s is missing from the plist\n", relative_path);
    }

    return 0;
}

static void 
check_for_missing_files(const char *destdir, const char *prefix, mportAssetList *assetlist) 
{
    global_destdir = destdir;
    global_prefix = prefix;
    global_assetlist = assetlist;

	if (global_destdir == NULL || global_prefix == NULL) {
		(void)fprintf(stderr, "Error: global_destdir or global_prefix is not initialized\n");
		exit(EXIT_FAILURE);
	}

	(void)printf("Checking for missing files. NOTE: may have false positives if plist uses @cwd\n");

    // Use nftw to traverse the destdir
    if (nftw(destdir, check_missing_from_plist, 10, FTW_PHYS) == -1) {
        perror("nftw");
        exit(EXIT_FAILURE);
    }
}
			
static void
usage(void) 
{
	errx(EX_USAGE, "Usage: mport.check-fake [-s skip] [-c <chroot directory>] <-f plistfile> <-d destdir> <-p prefix>");
}



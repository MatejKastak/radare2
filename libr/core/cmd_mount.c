/* radare - LGPL - Copyright 2009-2019 // pancake */

#define ms_argc (sizeof (ms_argv) / sizeof(const char*) - 1)
static const char *ms_argv[] = {
	"?", "!", "ls", "cd", "cat", "get", "mount", "help", "q", "exit", NULL
};

static const char *help_msg_m[] = {
	"Usage:", "m[-?*dgy] [...] ", "Mountpoints management",
	"m", "", "List all mountpoints in human readable format",
	"m*", "", "Same as above, but in r2 commands",
	"mj", "", "List mounted filesystems in JSON",
	"mL", "", "List filesystem plugins (Same as Lm)",
	"m", " /mnt", "Mount fs at /mnt with autodetect fs and current offset",
	"m", " /mnt ext2 0", "Mount ext2 fs at /mnt with delta 0 on IO",
	"m-/", "", "Umount given path (/)",
	"mc", "[file]", "Cat: Show the contents of the given file",
	"md", " /", "List directory contents for path",
	"mf", "[?] [o|n]", "Search files for given filename or for offset",
	"mg", " /foo", "Get fs file/dir and dump it to disk",
	"mo", " /foo/bar", "Open given file into a malloc://",
	"mi", " /foo/bar", "Get offset and size of given file",
	"mp", "", "List all supported partition types",
	"mp", " msdos 0", "Show partitions in msdos format at offset 0",
	"ms", " /mnt", "Open filesystem prompt at /mnt",
	"mw", " [file] [data]", "Write data into file", // TODO: add mwf
	"my", "", "Yank contents of file into clipboard",
	//"TODO: support multiple mountpoints and RFile IO's (need io+core refactorn",
	NULL
};

static void cmd_mount_init(RCore *core) {
	DEFINE_CMD_DESCRIPTOR (core, m);
}

static int cmd_mkdir(void *data, const char *input) {
	char *res = r_syscmd_mkdir (input);
	if (res) {
		r_cons_print (res);
		free (res);
	}
	return 0;
}

static int cmd_mv(void *data, const char *input) {
	return r_syscmd_mv (input)? 1: 0;
}

static char *cwd = NULL;
static char * av[1024] = {NULL};
#define av_max 1024

static char **getFilesFor(RCore *core, const char *path, int *ac) {
	RFS *fs = core->fs;
	RListIter *iter;
	RFSFile *file;
	char *full_path;
	char *lpath = strdup (path);
	
	if (!lpath) {
		return NULL;
	}

	r_str_trim_head (lpath);
	if (lpath[0] != '/') {
		full_path = r_str_newf ("%s/%s", cwd, lpath);
	} else {
		full_path = strdup (lpath);
	}
	free (lpath);

	//eprintf ("autocompleting for path '%s'\n", full_path);

	RList *list = r_fs_dir (fs, full_path);
	int count = 0;
	if (list) {
		r_list_foreach (list, iter, file) {
			eprintf ("==> %c %s\n", file->type, file->name);
			if (count >= av_max) {
				break;
			}
			av[count++] = file->name;
		}
		r_list_free (list);
	}
	// autocomplete mountpoints
	// mountpoints if any
	RFSRoot *r;
	char *me = strdup (full_path);
	r_list_foreach (fs->roots, iter, r) {
		char *base = strdup (r->path);
		char *ls = (char *) r_str_lchr (base, '/');
		if (ls) {
			ls++;
			*ls = 0;
		}
		// TODO: adjust contents between //
		if (!strcmp (me, base)) {
			//eprintf ("m %s\n", (r->path && r->path[0]) ? r->path + 1: "");
			if (count >= av_max) {
				break;
			}
			av[count++] = r->path;
		}
		free (base);
	}
	free (me);
	free (full_path);
	av[count] = NULL;
	if (ac) {
		*ac = count;
	}
	av[3] = NULL;
	return av;
}

static int ms_autocomplete(RLineCompletion *completion, RLineBuffer *buf, RLinePromptType prompt_type, void *user) {
	const char *data = buf->data;
	r_line_completion_set (completion, ms_argc, ms_argv);
	if (!strncmp (data, "ls ", 3)
		|| !strncmp (data, "cd ", 3)
		|| !strncmp (data, "cat ", 4)
	 	|| !strncmp (data, "get ", 4)) {
		const char *file = strchr (data, ' ');
		if (file++) {
			//eprintf ("FILE (%s)\n", file);
			int tmp_argc = 0;
			// TODO: handle abs vs rel
			char **tmp_argv = getFilesFor (user, file, &tmp_argc);
			r_line_completion_set (completion, tmp_argc, (const char **)tmp_argv);
		}
		return true;
	}
	return false;
}

static const char *t2s(const char ch) {
	switch (ch) {
	case 'f': return "file";
	case 'd': return "directory";
	case 'm': return "mountpoint";
	}
	return "unknown";
}

static void cmd_mount_ls (RCore *core, const char *input) {
	bool isJSON = *input == 'j';
	RListIter *iter;
	RFSFile *file;
	RFSRoot *root;
	input = r_str_trim_ro (input + isJSON);
	RList *list = r_fs_dir (core->fs, input);
	PJ *pj = NULL;
	if (isJSON) {
		pj = pj_new ();
		pj_a (pj);
	}
	if (list) {
		r_list_foreach (list, iter, file) {
			if (isJSON) {
				pj_o (pj);
				pj_ks (pj, "type", t2s(file->type));
				pj_ks (pj, "name", file->name);
				pj_end (pj);
			} else {
				r_cons_printf ("%c %s\n", file->type, file->name);
			}
		}
		r_list_free (list);
	}
	const char *path = *input? input: "/";
	r_list_foreach (core->fs->roots, iter, root) {
		// TODO: adjust contents between //
		if (!strncmp (path, root->path, strlen (path))) {
			char *base = strdup (root->path);
			char *ls = (char *)r_str_lchr (base, '/');
			if (ls) {
				ls++;
				*ls = 0;
			}
			// TODO: adjust contents between //
			if (!strcmp (path, base)) {
				if (isJSON) {
					pj_o (pj);
					pj_ks (pj, "path", root->path);
					pj_ks (pj, "type", "mountpoint");
					pj_end (pj);
				} else {
					r_cons_printf ("m %s\n", root->path); //  (root->path && root->path[0]) ? root->path + 1: "");
				}
			}
			free (base);
		}
	}
	if (isJSON) {
		pj_end (pj);
		r_cons_printf ("%s\n", pj_string (pj));
		pj_free (pj);
	}
}

static int cmd_mount(void *data, const char *_input) {
	ut64 off = 0;
	char *input, *oinput, *ptr, *ptr2;
	RList *list;
	RListIter *iter;
	RFSFile *file;
	RFSRoot *root;
	RFSPlugin *plug;
	RFSPartition *part;
	RCore *core = (RCore *)data;

	if (!strncmp ("kdir", _input, 4)) {
		return cmd_mkdir (data, _input);
	}
	if (!strncmp ("v", _input, 1)) {
		return cmd_mv (data, _input);
	}
	input = oinput = strdup (_input);

	switch (*input) {
	case ' ':
		input = (char *)r_str_trim_ro (input + 1);
		ptr = strchr (input, ' ');
		if (ptr) {
			*ptr = 0;
			ptr = (char *)r_str_trim_ro (ptr + 1);
			ptr2 = strchr (ptr, ' ');
			if (ptr2) {
				*ptr2 = 0;
				off = r_num_math (core->num, ptr2+1);
			}
			input = (char *)r_str_trim_ro (input);
			ptr = (char*)r_str_trim_ro (ptr);
			if (!r_fs_mount (core->fs, input, ptr, off)) {
				if (!r_fs_mount (core->fs, ptr, input, off)) {
					eprintf ("Cannot mount %s\n", input);
				}
			}
		} else {
			if (!(ptr = r_fs_name (core->fs, core->offset))) {
				eprintf ("Unknown filesystem type\n");
			}
			if (!r_fs_mount (core->fs, ptr, input, core->offset)) {
				eprintf ("Cannot mount %s\n", input);
			}
			free (ptr);
		}
		break;
	case '-':
		r_fs_umount (core->fs, input+1);
		break;
	case 'j':
		{
			PJ *pj = pj_new ();
			pj_o (pj);
			pj_k (pj, "mountpoints");
			pj_a (pj);
			r_list_foreach (core->fs->roots, iter, root) {
				pj_o (pj);
				pj_ks (pj, "path", root->path);
				pj_ks (pj, "plugin", root->p->name);
				pj_kn (pj, "offset", root->delta);
				pj_end (pj);
			}
			pj_end (pj);
//
			pj_k (pj, "plugins");
			pj_a (pj);
			r_list_foreach (core->fs->plugins, iter, plug) {
				pj_o (pj);
				pj_ks (pj, "name", plug->name);
				pj_ks (pj, "description", plug->desc);
				pj_end (pj);
			}

			pj_end (pj);
			pj_end (pj);
			r_cons_printf ("%s\n", pj_string (pj));
			pj_free (pj);
		}
		break;
	case '*':
		r_list_foreach (core->fs->roots, iter, root) {
			r_cons_printf ("m %s %s 0x%"PFMT64x"\n",
				root-> path, root->p->name, root->delta);
		}
		break;
	case '\0':
		r_list_foreach (core->fs->roots, iter, root) {
			r_cons_printf ("%s\t0x%"PFMT64x"\t%s\n",
				root->p->name, root->delta, root->path);
		}
		break;
	case 'L': // "mL" list of plugins
		r_list_foreach (core->fs->plugins, iter, plug) {
			r_cons_printf ("%10s  %s\n", plug->name, plug->desc);
		}
		break;
	case 'l': // "ml"
	case 'd': // "md" // should be deprecated. ls is better than dir :P
		cmd_mount_ls (core, input + 1);
		break;
	case 'p':
		input++;
		if (*input == ' ') {
			input++;
		}
		ptr = strchr (input, ' ');
		if (ptr) {
			*ptr = 0;
			off = r_num_math (core->num, ptr+1);
		}
		list = r_fs_partitions (core->fs, input, off);
		if (list) {
			r_list_foreach (list, iter, part) {
				r_cons_printf ("%d %02x 0x%010"PFMT64x" 0x%010"PFMT64x"\n",
					part->number, part->type,
					part->start, part->start+part->length);
			}
			r_list_free (list);
		} else {
			eprintf ("Cannot read partition\n");
		}
		break;
	case 'o': //"mo"
		input++;
		if (input[0]==' ') {
			input++;
		}
		file = r_fs_open (core->fs, input, false);
		if (file) {
			r_fs_read (core->fs, file, 0, file->size);
			char *uri = r_str_newf ("malloc://%d", file->size);
			RIODesc *fd = r_io_open (core->io, uri, R_PERM_RW, 0);
			if (fd) {
				r_io_desc_write (fd, file->data, file->size);
			}
		} else {
			eprintf ("Cannot open file\n");
		}
		break;
	case 'i':
		input++;
		if (input[0]==' ') {
			input++;
		}
		file = r_fs_open (core->fs, input, false);
		if (file) {
			// XXX: dump to file or just pipe?
			r_fs_read (core->fs, file, 0, file->size);
			r_cons_printf ("f file %d 0x%08"PFMT64x"\n", file->size, file->off);
			r_fs_close (core->fs, file);
		} else {
			eprintf ("Cannot open file\n");
		}
		break;
	case 'c': // "mc"
		input++;
		if (*input == ' ') {
			input++;
		}
		ptr = strchr (input, ' ');
		if (ptr) {
			*ptr++ = 0;
		} else {
			ptr = "./";
		}
		file = r_fs_open (core->fs, input, false);
		if (file) {
			r_fs_read (core->fs, file, 0, file->size);
			r_cons_memcat ((const char *)file->data, file->size);
			r_fs_close (core->fs, file);
			r_cons_memcat ("\n", 1);
		} else if (!r_fs_dir_dump (core->fs, input, ptr)) {
			eprintf ("Cannot open file\n");
		}
		break;
	case 'g': // "mg"
		input++;
		if (*input == ' ') {
			input++;
		}
		ptr = strchr (input, ' ');
		if (ptr) {
			*ptr++ = 0;
		} else {
			ptr = "./";
		}
		file = r_fs_open (core->fs, input, false);
		if (file) {
			char *localFile = strdup (input);
			char *slash = (char *)r_str_rchr (localFile, NULL, '/');
			if (slash) {
				memmove (localFile, slash + 1, strlen (slash));
			}
			r_fs_read (core->fs, file, 0, file->size);
			r_file_dump (localFile, file->data, file->size, false);
			r_fs_close (core->fs, file);
			eprintf ("File '%s' created.\n", localFile);
			free (localFile);
		} else if (!r_fs_dir_dump (core->fs, input, ptr)) {
			eprintf ("Cannot open file\n");
		}
		break;
	case 'f':
		input++;
		switch (*input) {
		case '?':
			r_cons_printf (
			"Usage: mf[no] [...]\n"
			" mfn /foo *.c       ; search files by name in /foo path\n"
			" mfo /foo 0x5e91    ; search files by offset in /foo path\n"
			);
			break;
		case 'n':
			input++;
			if (*input == ' ')
				input++;
			ptr = strchr (input, ' ');
			if (ptr) {
				*ptr++ = 0;
				list = r_fs_find_name (core->fs, input, ptr);
				r_list_foreach (list, iter, ptr) {
					r_str_trim_path (ptr);
					printf ("%s\n", ptr);
				}
				//XXX: r_list_purge (list);
			} else eprintf ("Unknown store path\n");
			break;
		case 'o':
			input++;
			if (*input == ' ')
				input++;
			ptr = strchr (input, ' ');
			if (ptr) {
				*ptr++ = 0;
				ut64 off = r_num_math (core->num, ptr);
				list = r_fs_find_off (core->fs, input, off);
				r_list_foreach (list, iter, ptr) {
					r_str_trim_path (ptr);
					printf ("%s\n", ptr);
				}
				//XXX: r_list_purge (list);
			} else eprintf ("Unknown store path\n");
			break;
		}
		break;
	case 's': // "ms"
		if (core->http_up) {
			free (oinput);
			return false;
		}
		input++;
		if (input[0] == ' ') {
			input++;
		}
		r_cons_set_raw (false);
		{
			RFSShell shell = {
				.cwd = &cwd,
				.set_prompt = r_line_set_prompt,
				.readline = r_line_readline,
				.hist_add = r_line_hist_add
			};
			RLine *rli = r_line_singleton ();
			RLineCompletion c;
			memcpy (&c, &rli->completion, sizeof (c));
			r_pvector_init (&rli->completion.args, free);  // UGLY HACK
			rli->completion.run = ms_autocomplete;
			rli->completion.run_user = rli->user;
			r_line_completion_set (&rli->completion, ms_argc, ms_argv);
			r_fs_shell_prompt (&shell, core->fs, input);
			R_FREE (cwd);
			r_pvector_clear (&rli->completion.args);
			memcpy (&rli->completion, &c, sizeof (c));
		}
		break;
	case 'w':
		if (input[1] == ' ') {
			char *args = r_str_trim_dup (input + 1);
			char *arg = strchr (args, ' ');
			if (arg) {
				data = arg + 1;
			} else {
				data = "";
				// touch and truncate
			}
			RFSFile *f = r_fs_open (core->fs, args, true);
			if (f) {
				r_fs_write (core->fs, f, 0, (const ut8 *)data, strlen (data));
				r_fs_close (core->fs, f);
				r_fs_file_free (f);
				free (args);
			}
		} else {
			eprintf ("Usage: mw [file] ([data])\n");
		}
		break;
	case 'y':
		eprintf ("TODO\n");
		break;
	case '?':
		r_core_cmd_help (core, help_msg_m);
		break;
	}
	free (oinput);
	return 0;
}

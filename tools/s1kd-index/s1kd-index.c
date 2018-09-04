#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxslt/transform.h>
#include "xslt.h"
#include "s1kd_tools.h"

#define PROG_NAME "s1kd-index"
#define VERSION "1.3.2"

/* Path to text nodes where indexFlags may occur */
#define ELEMENTS_XPATH BAD_CAST "//para/text()"

#define PRE_TERM_DELIM BAD_CAST " "
#define POST_TERM_DELIM BAD_CAST " .,"

/* Bug in libxml < 2.9.2 where parameter entities are resolved even when
 * XML_PARSE_NOENT is not specified.
 */
#if LIBXML_VERSION < 20902
#define PARSE_OPTS XML_PARSE_NONET
#else
#define PARSE_OPTS 0
#endif

#define ERR_PREFIX PROG_NAME ": ERROR: "
#define E_NO_LIST ERR_PREFIX "Could not read index flags from %s\n"
#define E_BAD_LIST ERR_PREFIX "Could not read list: %s\n"
#define E_NO_FILE ERR_PREFIX "Could not read file: %s\n"
#define EXIT_NO_LIST 1

/* Help/usage message */
void show_help(void)
{
	puts("Usage:");
	puts("  " PROG_NAME " -h?");
	puts("  " PROG_NAME " [-I <index>] [-fi] [<module>...]");
	puts("  " PROG_NAME " -D [-fi] [<module>...]");
	puts("");
	puts("Options:");
	puts("  -D          Delete current index flags.");
	puts("  -f          Overwrite input module(s).");
	puts("  -I <index>  Specify a custom .indexflags file");
	puts("  -i          Ignore case when flagging terms.");
	puts("  -h -?       Show help/usage message.");
	puts("  --version   Show version information.");
}

void show_version(void)
{
	printf("%s (s1kd-tools) %s\n", PROG_NAME, VERSION);
	printf("Using libxml %s and libxslt %s\n", xmlParserVersion, xsltEngineVersion);
}

/* Return the lowest level in an indexFlag. This is matched against the text
 * to determine where to insert the flag.
 */
xmlChar *last_level(xmlNodePtr flag)
{
	xmlChar *lvl;

	if ((lvl = xmlGetProp(flag, BAD_CAST "indexLevelFour"))) {
		return lvl;
	} else if ((lvl = xmlGetProp(flag, BAD_CAST "indexLevelThree"))) {
		return lvl;
	} else if ((lvl = xmlGetProp(flag, BAD_CAST "indexLevelTwo"))) {
		return lvl;
	} else if ((lvl = xmlGetProp(flag, BAD_CAST "indexLevelOne"))) {
		return lvl;
	}

	return NULL;
}

bool is_term(xmlChar *content, int content_len, int i, xmlChar *term, int term_len, bool ignorecase)
{
	bool is;
	xmlChar s, e;

	s = i == 0 ? ' ' : content[i - 1];
	e = i + term_len >= content_len - 1 ? ' ' : content[i + term_len];

	is = xmlStrchr(PRE_TERM_DELIM, s) &&
	     (ignorecase ?
	     	xmlStrncasecmp(content + i, term, term_len) :
		xmlStrncmp(content + i, term, term_len)) == 0 &&
	     xmlStrchr(POST_TERM_DELIM, e);

	return is;
}

/* Insert indexFlag elements after matched terms. */
void gen_index_node(xmlNodePtr node, xmlNodePtr flag, bool ignorecase)
{
	xmlChar *content;
	xmlChar *term;
	int term_len, content_len;
	int i;

	content = xmlNodeGetContent(node);
	content_len = xmlStrlen(content);

	term = last_level(flag);
	term_len = xmlStrlen(term);

	i = 0;
	while (i + term_len <= content_len) {
		if (is_term(content, content_len, i, term, term_len, ignorecase)) {
			xmlChar *s1 = xmlStrndup(content, i + term_len);
			xmlChar *s2 = xmlStrsub(content, i + term_len, content_len - (i + term_len));
			xmlNodePtr acr;

			xmlFree(content);

			xmlNodeSetContent(node, s1);
			xmlFree(s1);

			acr = xmlAddNextSibling(node, xmlCopyNode(flag, 1));
			node = xmlAddNextSibling(acr, xmlNewText(s2));

			content = s2;
			content_len = xmlStrlen(s2);
			i = 0;
		} else {
			++i;
		}
	}

	xmlFree(term);
	xmlFree(content);
}

/* Flag an individual term in all applicable elements in a module. */
void gen_index_flag(xmlNodePtr flag, xmlXPathContextPtr ctx, bool ignorecase)
{
	xmlXPathObjectPtr obj;

	obj = xmlXPathEvalExpression(ELEMENTS_XPATH, ctx);

	if (!xmlXPathNodeSetIsEmpty(obj->nodesetval)) {
		int i;

		for (i = 0; i < obj->nodesetval->nodeNr; ++i) {
			gen_index_node(obj->nodesetval->nodeTab[i], flag, ignorecase);
		}
	}

	xmlXPathFreeObject(obj);
}

/* Insert indexFlags for each term included in the specified index file. */
void gen_index_flags(xmlNodeSetPtr flags, xmlXPathContextPtr ctx, bool ignorecase)
{
	int i;

	for (i = 0; i < flags->nodeNr; ++i) {
		gen_index_flag(flags->nodeTab[i], ctx, ignorecase);
	}
}

/* Apply a built-in XSLT transform to a doc in place. */
void transform_doc(xmlDocPtr doc, unsigned char *xsl, unsigned int len)
{
	xmlDocPtr styledoc, src, res;
	xsltStylesheetPtr style;
	xmlNodePtr old;

	src = xmlCopyDoc(doc, 1);

	styledoc = xmlReadMemory((const char *) xsl, len, NULL, NULL, 0);
	style = xsltParseStylesheetDoc(styledoc);

	res = xsltApplyStylesheet(style, src, NULL);

	old = xmlDocSetRootElement(doc, xmlCopyNode(xmlDocGetRootElement(res), 1));
	xmlFreeNode(old);

	xmlFreeDoc(src);
	xmlFreeDoc(res);
	xsltFreeStylesheet(style);
}

/* Convert index flags for older issues. */
void convert_to_iss_30(xmlDocPtr doc)
{
	transform_doc(doc, iss30_xsl, iss30_xsl_len);
}

void delete_index_flags(const char *path, bool overwrite)
{
	xmlDocPtr doc;

	doc = xmlReadFile(path, NULL, PARSE_OPTS);

	transform_doc(doc, delete_xsl, delete_xsl_len);

	if (overwrite) {
		xmlSaveFile(path, doc);
	} else {
		xmlSaveFile("-", doc);
	}
}

/* Insert indexFlag elements after matched terms in a document. */
void gen_index(const char *path, xmlDocPtr index_doc, bool overwrite, bool ignorecase)
{
	xmlDocPtr doc;
	xmlXPathContextPtr doc_ctx, index_ctx;
	xmlXPathObjectPtr index_obj;
	xmlNodeSetPtr flags;

	if (!(doc = xmlReadFile(path, NULL, PARSE_OPTS))) {
		fprintf(stderr, E_NO_FILE, path);
		return;
	}

	index_ctx = xmlXPathNewContext(index_doc);
	index_obj = xmlXPathEvalExpression(BAD_CAST "//indexFlag", index_ctx);
	flags = index_obj->nodesetval;

	doc_ctx = xmlXPathNewContext(doc);

	if (!xmlXPathNodeSetIsEmpty(flags)) {
		gen_index_flags(flags, doc_ctx, ignorecase);
	}

	xmlXPathFreeContext(doc_ctx);
	xmlXPathFreeObject(index_obj);
	xmlXPathFreeContext(index_ctx);

	if (xmlStrcmp(xmlFirstElementChild(xmlDocGetRootElement(doc))->name, BAD_CAST "idstatus") == 0) {
		convert_to_iss_30(doc);
	}

	if (overwrite) {
		xmlSaveFile(path, doc);
	} else {
		xmlSaveFile("-", doc);
	}

	xmlFreeDoc(doc);
}

xmlDocPtr read_index_flags(const char *fname)
{
	xmlDocPtr index_doc;

	if (!(index_doc = xmlReadFile(fname, NULL, PARSE_OPTS))) {
		fprintf(stderr, E_NO_LIST, fname);
		exit(EXIT_NO_LIST);
	}

	return index_doc;
}

void handle_list(const char *path, bool delflags, xmlDocPtr index_doc, bool overwrite, bool ignorecase)
{
	FILE *f;
	char line[PATH_MAX];

	if (path) {
		f = fopen(path, "r");
	} else {
		f = stdin;
	}

	if (!f) {
		fprintf(stderr, E_BAD_LIST, path);
		return;
	}

	while (fgets(line, PATH_MAX, f)) {
		strtok(line, "\t\r\n");

		if (delflags) {
			delete_index_flags(line, overwrite);
		} else {
			gen_index(line, index_doc, overwrite, ignorecase);
		}
	}

	fclose(f);
}

char *real_path(const char *path, char *real)
{
	#ifdef _WIN32
	if (!GetFullPathName(path, PATH_MAX, real, NULL)) {
	#else
	if (!realpath(path, real)) {
	#endif
		strcpy(real, path);
	}
	return real;
}

/* Search up the directory tree to find a configuration file. */
int find_config(char *dst, const char *name)
{
	char cwd[PATH_MAX], prev[PATH_MAX];
	bool found = true;

	real_path(".", cwd);
	strcpy(prev, cwd);

	while (access(name, F_OK) == -1) {
		char cur[PATH_MAX];

		if (chdir("..") || strcmp(real_path(".", cur), prev) == 0) {
			found = false;
			break;
		}

		strcpy(prev, cur);
	}

	if (found) {
		real_path(name, dst);
	} else {
		strcpy(dst, name);
	}

	return chdir(cwd);
}

int main(int argc, char **argv)
{
	int i;
	bool overwrite = false;
	bool ignorecase = false;
	bool delflags = false;
	bool list = false;

	xmlDocPtr index_doc = NULL;

	const char *sopts = "DfI:lih?";
	struct option lopts[] = {
		{"version", no_argument, 0, 0},
		{0, 0, 0, 0}
	};
	int loptind = 0;

	while ((i = getopt_long(argc, argv, sopts, lopts, &loptind)) != -1) {
		switch (i) {
			case 0:
				if (strcmp(lopts[loptind].name, "version") == 0) {
					show_version();
					return 0;
				}
				break;
			case 'D':
				delflags = true;
				break;
			case 'f':
				overwrite = true;
				break;
			case 'I':
				if (!index_doc) {
					index_doc = read_index_flags(optarg);
				}
				break;
			case 'i':
				ignorecase = true;
				break;
			case 'l':
				list = true;
				break;
			case 'h':
			case '?':
				show_help();
				return 0;
		}
	}

	if (!index_doc) {
		char fname[PATH_MAX];
		find_config(fname, DEFAULT_INDEXFLAGS_FNAME);
		index_doc = read_index_flags(fname);
	}

	if (optind < argc) {
		for (i = optind; i < argc; ++i) {
			if (list) {
				handle_list(argv[i], delflags, index_doc, overwrite, ignorecase);
			} else if (delflags) {
				delete_index_flags(argv[i], overwrite);
			} else {
				gen_index(argv[i], index_doc, overwrite, ignorecase);
			}
		}
	} else if (list) {
		handle_list(NULL, delflags, index_doc, overwrite, ignorecase);
	} else if (delflags) {
		delete_index_flags("-", false);
	} else {
		gen_index("-", index_doc, false, ignorecase);
	}

	xmlFreeDoc(index_doc);

	xsltCleanupGlobals();
	xmlCleanupParser();

	return 0;
}

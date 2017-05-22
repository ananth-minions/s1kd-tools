#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <libxml/tree.h>
#include <libxml/xmlschemas.h>
#include <libxml/debugXML.h>

#define PROGNAME "s1kd-validate"

#define ERR_PREFIX PROGNAME ": ERROR: "
#define SUCCESS_PREFIX PROGNAME ": SUCCESS: "
#define FAILED_PREFIX PROGNAME ": FAILED: "

#define EXIT_MAX_SCHEMAS 1
#define EXIT_MISSING_SCHEMA 2

enum verbosity_level {SILENT, NORMAL, VERBOSE, DEBUG} verbosity = NORMAL;

/* Cache schemas to prevent parsing them twice (mainly needed when accessing
 * the schema over a network)
 */
struct s1kd_schema_parser {
	char *url;
	xmlSchemaParserCtxtPtr ctxt;
	xmlSchemaPtr schema;
	xmlSchemaValidCtxtPtr valid_ctxt;
};

#define SCHEMA_PARSERS_MAX 32

struct s1kd_schema_parser schema_parsers[SCHEMA_PARSERS_MAX];

int schema_parser_count = 0;

struct s1kd_schema_parser *get_schema_parser(const char *url)
{
	int i;

	for (i = 0; i < schema_parser_count; ++i) {
		if (strcmp(schema_parsers[i].url, url) == 0) {
			return &(schema_parsers[i]);
		}
	}

	return NULL;
}

struct s1kd_schema_parser *add_schema_parser(char *url)
{
	struct s1kd_schema_parser *parser;

	xmlSchemaParserCtxtPtr ctxt;
	xmlSchemaPtr schema;
	xmlSchemaValidCtxtPtr valid_ctxt;

	ctxt = xmlSchemaNewParserCtxt(url);
	schema = xmlSchemaParse(ctxt);
	valid_ctxt = xmlSchemaNewValidCtxt(schema);

	xmlSchemaSetParserErrors(ctxt,
		(xmlSchemaValidityErrorFunc) fprintf,
		(xmlSchemaValidityWarningFunc) fprintf,
		stderr);
	xmlSchemaSetValidErrors(valid_ctxt,
		(xmlSchemaValidityErrorFunc) fprintf,
		(xmlSchemaValidityWarningFunc) fprintf,
		stderr);

	schema_parsers[schema_parser_count].url = url;
	schema_parsers[schema_parser_count].ctxt = ctxt;
	schema_parsers[schema_parser_count].schema = schema;
	schema_parsers[schema_parser_count].valid_ctxt = valid_ctxt;

	parser = &schema_parsers[schema_parser_count];

	++schema_parser_count;

	return parser;
}

void show_help(void)
{
	puts("Usage: " PROGNAME " [-d <dir>] [-vqD] <dms>");
	puts("");
	puts("Options:");
	puts("  -d <dir> Search for schemas in <dir> instead of using the URL.");
	puts("  -v       Verbose output.");
	puts("  -q       Silent (not output).");
	puts("  -D       Debug output.");
	puts("  <dms>    Any number of data modules to validate.");
}

void validate_file(const char *fname, const char *schema_dir)
{
	xmlDocPtr doc;
	xmlNodePtr dmodule;
	char *url;
	struct s1kd_schema_parser *parser;
	int err;

	doc = xmlReadFile(fname, NULL, 0);

	dmodule = xmlDocGetRootElement(doc);

	url = (char *) xmlGetProp(dmodule, (xmlChar *) "noNamespaceSchemaLocation");

	if (!url) {
		if (verbosity > SILENT) {
			fprintf(stderr, ERR_PREFIX "%s has no schema.\n", fname);
		}
		return;
	}

	if (strcmp(schema_dir, "") != 0) {
		char *last_slash, *schema_name, schema_file[256];

		last_slash = strrchr(url, '/');
		schema_name = url + (last_slash - url) + 1;
		snprintf(schema_file, 256, "%s/%s", schema_dir, schema_name);

		if (access(schema_file, F_OK) == -1) {
			if (verbosity > SILENT) {
				fprintf(stderr, ERR_PREFIX "Schema %s not found in %s.\n", schema_name, schema_dir);
			}
			exit(EXIT_MISSING_SCHEMA);
		}

		xmlFree(url);
		url = (char *) xmlStrdup((xmlChar *) schema_file);
	}

	if (!(parser = get_schema_parser(url))) {
		if (schema_parser_count == SCHEMA_PARSERS_MAX) {
			if (verbosity > SILENT) {
				fprintf(stderr, ERR_PREFIX "Maximum number of schemas reached.\n");
			}
			exit(EXIT_MAX_SCHEMAS);
		}

		parser = add_schema_parser(url);
	}

	err = xmlSchemaValidateDoc(parser->valid_ctxt, doc);

	if (verbosity >= VERBOSE) {
		if (err) {
			printf(FAILED_PREFIX "%s fails to validate\n", fname);
		} else {
			printf(SUCCESS_PREFIX "%s validates\n", fname);
		}
	}

	xmlFreeDoc(doc);
}

int main(int argc, char *argv[])
{
	int c, i;
	char schema_dir[256] = "";

	while ((c = getopt(argc, argv, "vqDd:h?")) != -1) {
		switch (c) {
			case 's': verbosity = SILENT; break;
			case 'v': verbosity = VERBOSE; break;
			case 'D': verbosity = DEBUG; break;
			case 'd': strcpy(schema_dir, optarg); break;
			case 'h':
			case '?': show_help(); exit(0);
		}
	}

	if (optind >= argc) {
		validate_file("-", schema_dir);
	} else {
		for (i = optind; i < argc; ++i) {
			validate_file(argv[i], schema_dir);
		}
	}

	for (i = 0; i < schema_parser_count; ++i) {
		xmlFree(schema_parsers[i].url);
		xmlSchemaFreeValidCtxt(schema_parsers[i].valid_ctxt);
		xmlSchemaFree(schema_parsers[i].schema);
		xmlSchemaFreeParserCtxt(schema_parsers[i].ctxt);
	}

	xmlCleanupParser();

	return 0;
}